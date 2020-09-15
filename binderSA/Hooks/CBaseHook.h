#pragma once

/*
	По неизвестной причине, C++ не конвертирует указатель на метод класса в обычный указатель.
	Собственная реализация конвертирования от BlackKnigga служит для исправления данного недуга.
*/
template <typename Out, typename In>
inline Out force_cast(In in) {
	union {
		In  in;
		Out out;
	} u = { in };

	return u.out;
};

/*
	Перечисление типов хука:
		Redirect - обычная подмена параметров в JMP/CALL.
		Trampoline - хук с сохранением эпилога и дальнейшим его выполнением.
*/
enum hookTypes {
	redirectHook,
	trampolineHook,
};

// Методы хука для типа Redirect. Думаю, не нуждается в объяснении. (Рекомендуется: call_method)
enum hookMethods {
	callMethod = 0xE8,
	jumpMethod = 0xE9,
};

/*
	Класс служит для управления защитой памяти
	определенного региона.
*/
class CUnprotectRegion {
public:
	/*
		regionToUnprotect - адрес памяти для снятия протекции.
		regionSize - размер региона памяти.
	*/
	CUnprotectRegion(void *regionToUnprotect, uint32_t regionSize) {
		memoryRegion = regionToUnprotect;
		sizeOfRegion = regionSize;

		VirtualProtect(memoryRegion, sizeOfRegion, PAGE_EXECUTE_READWRITE, &oldProtection);
	}
	~CUnprotectRegion() {
		VirtualProtect(memoryRegion, sizeOfRegion, oldProtection, &oldProtection);
	}

private:
	DWORD oldProtection; // Контейнер протекции для возврата на место.
	void *memoryRegion; // Регион памяти.
	uint32_t sizeOfRegion; // Размер региона.
};

class CBaseHook {
public:
	/*
		sourceFunction - адрес на функцию, которую вы собираетесь хукать. 
		В случае с типом хука Redirect, адрес команды JMP/CALL.
		destinationFunction - функция, которая будет принимать на
		себя параметры хука.
		prologueLength - длина (размер) пролога функции. 
		Если тип хука Redirect, поставьте 0.
		hookType - тип хука.
		hookMethod - метод хука (имеет разницу для типа хука redirect).
	*/
	template <class Source, class Destination>
	CBaseHook(Source sourceFunction, Destination destinationFunction,
		uint32_t prologueLength,
		uint32_t _hookType = trampolineHook,
		uint8_t hookMethod = jumpMethod)
	{
		hookType = _hookType;
		sourcePointer = reinterpret_cast<void *>(sourceFunction); // Представляем адрес в виде указателя.
		destinationPointer = force_cast<void *>(destinationFunction); // Представляем функцию, которая будет заменять оригинальную, в виде указателя.

		if (hookType == redirectHook) // Если тип хука - Redirect:
		{
			CUnprotectRegion regionProtect(sourcePointer, defaultInstructionSize); // Инициализируем класс unprotect_region. Снимаем защиту с региона памяти.

			originalInstructions.first = *reinterpret_cast<uint8_t *>(
				reinterpret_cast<uint32_t>(sourcePointer)); // Записываем в пару оригинальный опкод команды.
			originalInstructions.second = *reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01); // Записываем в пару оригинальный релативный адрес.

			std::memset(sourcePointer, noOperationOpcode, defaultInstructionSize); // Ноплю всю инструкцию JMP/CALL, её статичный размер для x86 - 5 байт.
			*reinterpret_cast<uint8_t *>(sourcePointer) = hookMethod; // Меняем оригинальный опкод на опкод команды, которую мы указали в параметрах.

			uint32_t relativeAddress = 
				reinterpret_cast<uint32_t>(destinationPointer) - 
				reinterpret_cast<uint32_t>(sourcePointer) - 5; // Вычисляем релативный адрес прыжка от pointer_on_source до pointer_on_destination.
															   // Если говорить проще - прыгаем с оригинальной функции на нашу.

			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01) = relativeAddress; // Перезаписываем релативный адрес на собственный.

			regionProtect.~CUnprotectRegion(); // Вызываем деструктор класса, восстанавливаем оригинальный уровень протекции региона.
		} 
		else if (hookType == trampolineHook) // Иначе, если же тип хука - Trampoline.
		{ 
			if (prologueLength < 5) { // РАЗМЕР ПРОЛОГА НЕ МОЖЕТ БЫТЬ МЕНЬШЕ 5! Запомните это раз и навсегда.
				return;
			}

			lengthOfPrologue = prologueLength;

			/* 
				Выделяем виртуальную память размером с размер пролога + 5. 
				Представим ситуацию, мы хукаем функцию, у которой размер пролога 10,
				но мы выделяем 10 + 5, а не 10. Почему?
				Потому-что в первые 10 байт запишется пролог, а в остальные 5 байт
				запишется прыжок на оригинальную функцию, чтобы не сохранять в трамплин весь ее код.
			*/
			gatewayPointer = VirtualAlloc(nullptr, prologueLength + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(gatewayPointer, sourcePointer, prologueLength); // Копируем байты пролога в указатель.

			uint32_t relativeAddress =
				reinterpret_cast<uint32_t>(sourcePointer) -
				reinterpret_cast<uint32_t>(gatewayPointer) - 5; // Вычисляем релативный адрес прыжка от трамплина до оригинальной функции.

			*reinterpret_cast<uint8_t *>(
				reinterpret_cast<uint32_t>(gatewayPointer) +
				prologueLength) = 0xE9; // Записываем опкод прыжка, +1 байт после пролога в трамплине.

			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(gatewayPointer) + 
				prologueLength + 0x01) = relativeAddress; // Записываем релативный адрес прыжка на ориг. функцию, +2 байта после пролога.

			CUnprotectRegion protectRegion(sourcePointer, prologueLength); // Инициализируем класс, снимаем защиту памяти.

			std::memset(sourcePointer, 0x90, prologueLength); // Обнуляем весь пролог оригинальной функции.
			*reinterpret_cast<uint8_t *>(sourcePointer) = 0xE9; // Подменяем первый байт пролога на опкод прыжка.

			relativeAddress =
				reinterpret_cast<uint32_t>(destinationPointer) -
				reinterpret_cast<uint32_t>(sourcePointer) - 5; // Вычисляем релативный адрес прыжка от оригинальной функции на нашу.

			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01) = relativeAddress; // Перезаписываем релативный адрес.

			protectRegion.~CUnprotectRegion(); // Восстанавливаем протекцию региона.

		}
	}

	// Деструктор класса, снимает хуки.
	~CBaseHook()
	{
		if (hookType == redirectHook) // Если тип хука - Redirect.
		{
			CUnprotectRegion protectRegion(sourcePointer, defaultInstructionSize); // Снимаем протекцию региона.

			*reinterpret_cast<uint8_t *>(sourcePointer) = originalInstructions.first; // Восстанавливаем бывший опкод.
			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01) = originalInstructions.second; // Восстанавливаем бывший релативный адрес.

			protectRegion.~CUnprotectRegion(); // Восстанавливаем протекцию.
		} 
		else if (hookType == trampolineHook) // Иначе же, если тип - Trampoline.
		{
			CUnprotectRegion protectRegion(sourcePointer, defaultInstructionSize); // Снимаем протекцию региона.

			memcpy(sourcePointer, gatewayPointer, lengthOfPrologue); // Копируем байты пролога из трамплина обратно в функцию.
			VirtualFree(gatewayPointer, NULL, MEM_RELEASE); // Освобождаем указатель на трамплин.

			protectRegion.~CUnprotectRegion(); // Восстанавливаем протекцию региона.
		}
	}

	// Функция кастит до определенного типа трамплин и возвращает.
	template <class T>
	T getTrampoline() {
		return reinterpret_cast<T>(gatewayPointer);
	}

private:
	void *sourcePointer;
	void *destinationPointer;
	void *gatewayPointer = nullptr;

	uint32_t defaultInstructionSize = 5;
	uint32_t noOperationOpcode = 0x90;
	uint32_t hookType;
	uint32_t lengthOfPrologue = 0;

	std::pair<uint8_t, uint32_t> originalInstructions;
};