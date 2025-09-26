#pragma once
#include <Windows.h>
class Dumper
{
public:
	static void startScan();
	static void stopScan();
	static void findSignatures(const BYTE* pattern, size_t, int);
	static void findSignaturesAsync(const BYTE* pattern, size_t, int);
};