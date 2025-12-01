import std::io;

i32 main()
{
	[u32] list = [1, 2, 3, 4, 5];

	// iterative loop
	loop (0.., list) |i, v| {
		printf("%d\n", i);
	}

	// conditional loop
	loop a == 3 {

	}

	// infinite loop
	loop {

	}
}
