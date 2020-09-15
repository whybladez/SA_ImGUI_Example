#pragma once

/*
	�� ����������� �������, C++ �� ������������ ��������� �� ����� ������ � ������� ���������.
	����������� ���������� ��������������� �� BlackKnigga ������ ��� ����������� ������� ������.
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
	������������ ����� ����:
		Redirect - ������� ������� ���������� � JMP/CALL.
		Trampoline - ��� � ����������� ������� � ���������� ��� �����������.
*/
enum hookTypes {
	redirectHook,
	trampolineHook,
};

// ������ ���� ��� ���� Redirect. �����, �� ��������� � ����������. (�������������: call_method)
enum hookMethods {
	callMethod = 0xE8,
	jumpMethod = 0xE9,
};

/*
	����� ������ ��� ���������� ������� ������
	������������� �������.
*/
class CUnprotectRegion {
public:
	/*
		regionToUnprotect - ����� ������ ��� ������ ���������.
		regionSize - ������ ������� ������.
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
	DWORD oldProtection; // ��������� ��������� ��� �������� �� �����.
	void *memoryRegion; // ������ ������.
	uint32_t sizeOfRegion; // ������ �������.
};

class CBaseHook {
public:
	/*
		sourceFunction - ����� �� �������, ������� �� ����������� ������. 
		� ������ � ����� ���� Redirect, ����� ������� JMP/CALL.
		destinationFunction - �������, ������� ����� ��������� ��
		���� ��������� ����.
		prologueLength - ����� (������) ������� �������. 
		���� ��� ���� Redirect, ��������� 0.
		hookType - ��� ����.
		hookMethod - ����� ���� (����� ������� ��� ���� ���� redirect).
	*/
	template <class Source, class Destination>
	CBaseHook(Source sourceFunction, Destination destinationFunction,
		uint32_t prologueLength,
		uint32_t _hookType = trampolineHook,
		uint8_t hookMethod = jumpMethod)
	{
		hookType = _hookType;
		sourcePointer = reinterpret_cast<void *>(sourceFunction); // ������������ ����� � ���� ���������.
		destinationPointer = force_cast<void *>(destinationFunction); // ������������ �������, ������� ����� �������� ������������, � ���� ���������.

		if (hookType == redirectHook) // ���� ��� ���� - Redirect:
		{
			CUnprotectRegion regionProtect(sourcePointer, defaultInstructionSize); // �������������� ����� unprotect_region. ������� ������ � ������� ������.

			originalInstructions.first = *reinterpret_cast<uint8_t *>(
				reinterpret_cast<uint32_t>(sourcePointer)); // ���������� � ���� ������������ ����� �������.
			originalInstructions.second = *reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01); // ���������� � ���� ������������ ���������� �����.

			std::memset(sourcePointer, noOperationOpcode, defaultInstructionSize); // ����� ��� ���������� JMP/CALL, � ��������� ������ ��� x86 - 5 ����.
			*reinterpret_cast<uint8_t *>(sourcePointer) = hookMethod; // ������ ������������ ����� �� ����� �������, ������� �� ������� � ����������.

			uint32_t relativeAddress = 
				reinterpret_cast<uint32_t>(destinationPointer) - 
				reinterpret_cast<uint32_t>(sourcePointer) - 5; // ��������� ���������� ����� ������ �� pointer_on_source �� pointer_on_destination.
															   // ���� �������� ����� - ������� � ������������ ������� �� ����.

			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01) = relativeAddress; // �������������� ���������� ����� �� �����������.

			regionProtect.~CUnprotectRegion(); // �������� ���������� ������, ��������������� ������������ ������� ��������� �������.
		} 
		else if (hookType == trampolineHook) // �����, ���� �� ��� ���� - Trampoline.
		{ 
			if (prologueLength < 5) { // ������ ������� �� ����� ���� ������ 5! ��������� ��� ��� � ��������.
				return;
			}

			lengthOfPrologue = prologueLength;

			/* 
				�������� ����������� ������ �������� � ������ ������� + 5. 
				���������� ��������, �� ������ �������, � ������� ������ ������� 10,
				�� �� �������� 10 + 5, � �� 10. ������?
				������-��� � ������ 10 ���� ��������� ������, � � ��������� 5 ����
				��������� ������ �� ������������ �������, ����� �� ��������� � �������� ���� �� ���.
			*/
			gatewayPointer = VirtualAlloc(nullptr, prologueLength + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			memcpy(gatewayPointer, sourcePointer, prologueLength); // �������� ����� ������� � ���������.

			uint32_t relativeAddress =
				reinterpret_cast<uint32_t>(sourcePointer) -
				reinterpret_cast<uint32_t>(gatewayPointer) - 5; // ��������� ���������� ����� ������ �� ��������� �� ������������ �������.

			*reinterpret_cast<uint8_t *>(
				reinterpret_cast<uint32_t>(gatewayPointer) +
				prologueLength) = 0xE9; // ���������� ����� ������, +1 ���� ����� ������� � ���������.

			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(gatewayPointer) + 
				prologueLength + 0x01) = relativeAddress; // ���������� ���������� ����� ������ �� ����. �������, +2 ����� ����� �������.

			CUnprotectRegion protectRegion(sourcePointer, prologueLength); // �������������� �����, ������� ������ ������.

			std::memset(sourcePointer, 0x90, prologueLength); // �������� ���� ������ ������������ �������.
			*reinterpret_cast<uint8_t *>(sourcePointer) = 0xE9; // ��������� ������ ���� ������� �� ����� ������.

			relativeAddress =
				reinterpret_cast<uint32_t>(destinationPointer) -
				reinterpret_cast<uint32_t>(sourcePointer) - 5; // ��������� ���������� ����� ������ �� ������������ ������� �� ����.

			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01) = relativeAddress; // �������������� ���������� �����.

			protectRegion.~CUnprotectRegion(); // ��������������� ��������� �������.

		}
	}

	// ���������� ������, ������� ����.
	~CBaseHook()
	{
		if (hookType == redirectHook) // ���� ��� ���� - Redirect.
		{
			CUnprotectRegion protectRegion(sourcePointer, defaultInstructionSize); // ������� ��������� �������.

			*reinterpret_cast<uint8_t *>(sourcePointer) = originalInstructions.first; // ��������������� ������ �����.
			*reinterpret_cast<uint32_t *>(
				reinterpret_cast<uint32_t>(sourcePointer) + 0x01) = originalInstructions.second; // ��������������� ������ ���������� �����.

			protectRegion.~CUnprotectRegion(); // ��������������� ���������.
		} 
		else if (hookType == trampolineHook) // ����� ��, ���� ��� - Trampoline.
		{
			CUnprotectRegion protectRegion(sourcePointer, defaultInstructionSize); // ������� ��������� �������.

			memcpy(sourcePointer, gatewayPointer, lengthOfPrologue); // �������� ����� ������� �� ��������� ������� � �������.
			VirtualFree(gatewayPointer, NULL, MEM_RELEASE); // ����������� ��������� �� ��������.

			protectRegion.~CUnprotectRegion(); // ��������������� ��������� �������.
		}
	}

	// ������� ������ �� ������������� ���� �������� � ����������.
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