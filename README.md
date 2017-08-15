# jsop
A fast JSON compatible parser.

Additional features not in JSON:
* Ignore comments
* Trailing comma before `]` or `}`
* Infinity/NaN literals
* Hexadecimal floating point numbers
* Unquoted object key (partial support)

Sample usage for parsing a string of size n:

	JsopParser<> parser;
	JsopDocument doc;
	
	if (parser.start()) {
		if (!parser.parse(str, n)) {
			return false;
		}
		return parser.finish(&doc);
	}

You can also call parse() multiple times if you are reading a large file into a fixed sized buffer.

Sample usage for packed values (which reduces the memory usage of the output data structure but reduces the maximum size of the input that can be parsed):

	JsopParser<JsopPackedDocumentHandler<JsopPackedAllocator<JsopPackedValue<uint64_t>>>> parser;
	JsopPackedDocument<JsopPackedValue<uint64_t>> doc;

	if (parser.start()) {
		if (!parser.parse(str, n)) {
			return false;
		}
		return parser.finish(&doc);
	}

## Benchmark
Compiled on Pentium G3258 for x86-64 with gcc 7.1.1:

	g++ -O2 -march=native -fno-exceptions -fno-rtti -DNDEBUG

| Time (ms) | RapidJSON | RapidJSON_Insitu | sajson | jsop packed 32-bit | jsop packed 64-bit | jsop |
| --- | --- | --- | --- | --- | --- | --- |
| canada.json | 4.354 | 4.801 | 4.815 | 4.783 | 4.891 | 4.543 |
| citm_catalog.json | 1.828 | 1.831 | 2.070 | 1.934 | 1.897 | 1.670 |
| twitter.json | 1.310 | 1.002 | 0.881 | 0.938 | 0.938 | 0.876 |
| apache_builds.json | 0.233 | 0.166 | 0.155 | 0.130 | 0.119 | 0.116 |
| instruments.json | 0.425 | 0.348 | 0.300 | 0.255 | 0.258 | 0.209 |
| mesh.json | 1.565 | 1.738 | 1.557 | 1.342 | 1.465 | 1.259 |
| update-center.json | 1.239 | 0.839 | 0.907 | 0.611 | 0.553 | 0.510 |
| harvard.json | 269.343 | 213.481 | 197.533 | 141.073 | 133.569 | 126.712 |
| citylots.json | 472.014 | 517.793 | 493.881 | 432.360 | 442.600 | 415.144 |
| random_uint.json | 244.675 | 267.852 | 281.314 | 175.251 | 196.273 | 144.920 |
| random_bool.json | 345.844 | 380.854 | 252.783 | 168.928 | 216.789 | 195.445 |
| random_int.json | 367.611 | 395.312 | 387.501 | 249.109 | 248.209 | 211.220 |

| Bandwidth (MB/s) | RapidJSON | RapidJSON_Insitu | sajson | jsop packed 32-bit | jsop packed 64-bit | jsop |
| --- | --- | --- | --- | --- | --- | --- |
| canada.json | 493.057 | 447.150 | 445.850 | 448.833 | 438.922 | 472.544 |
| citm_catalog.json | 901.089 | 899.612 | 795.744 | 851.701 | 868.313 | 986.341 |
| twitter.json | 459.739 | 601.057 | 683.608 | 642.067 | 642.067 | 687.510 |
| apache_builds.json | 520.939 | 731.198 | 783.090 | 933.684 | 1019.991 | 1046.370 |
| instruments.json | 494.443 | 603.846 | 700.461 | 824.072 | 814.490 | 1005.447 |
| mesh.json | 440.943 | 397.052 | 443.209 | 514.215 | 471.042 | 548.114 |
| update-center.json | 410.394 | 606.053 | 560.615 | 832.206 | 919.490 | 997.016 |
| harvard.json | 344.374 | 434.486 | 469.565 | 657.494 | 694.432 | 732.011 |
| citylots.json | 383.435 | 349.535 | 366.458 | 418.602 | 408.917 | 435.961 |
| random_uint.json | 417.001 | 380.918 | 362.690 | 582.192 | 519.836 | 704.042 |
| random_bool.json | 242.657 | 220.351 | 331.991 | 496.789 | 387.112 | 429.387 |
| random_int.json | 264.238 | 245.722 | 250.675 | 389.937 | 391.350 | 459.884 |
