int main()
{
	asm volatile ("bkpt #0" : : : "memory");
	while (1) {
		static long val;
		val++;
	}
}
