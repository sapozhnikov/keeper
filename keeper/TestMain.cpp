#include "stdafx.h"
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest\doctest.h"

int main(int argc, char** argv)
{
	sodium_init();

	doctest::Context context;
	context.applyCommandLine(argc, argv);

	return context.run();
}