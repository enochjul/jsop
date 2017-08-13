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
| canada.json | 4.352 | 4.788 | 4.818 | 4.594 | 4.731 | 4.480 |
| citm_catalog.json | 1.823 | 1.797 | 2.057 | 1.682 | 1.769 | 1.627 |
| twitter.json | 1.310 | 1.011 | 0.888 | 0.945 | 0.933 | 0.873 |
| apache_builds.json | 0.232 | 0.165 | 0.155 | 0.131 | 0.119 | 0.117 |
| instruments.json | 0.422 | 0.346 | 0.299 | 0.232 | 0.254 | 0.213 |
| mesh.json | 1.567 | 1.716 | 1.561 | 1.312 | 1.473 | 1.234 |
| update-center.json | 1.240 | 0.845 | 0.909 | 0.645 | 0.575 | 0.503 |
| harvard.json | 268.397 | 213.337 | 197.372 | 148.724 | 138.821 | 125.098 |
| citylots.json | 471.447 | 517.669 | 493.932 | 429.892 | 442.125 | 414.076 |
| random_uint.json | 244.563 | 267.663 | 281.128 | 174.955 | 195.749 | 145.291 |
| random_bool.json | 345.933 | 380.569 | 252.396 | 175.893 | 216.910 | 197.806 |
| random_int.json | 367.817 | 395.159 | 387.329 | 246.737 | 243.939 | 215.781 |

| Bandwidth (MB/s) | RapidJSON | RapidJSON_Insitu | sajson | jsop packed 32-bit | jsop packed 64-bit | jsop |
| --- | --- | --- | --- | --- | --- | --- |
| canada.json | 493.283 | 448.365 | 445.573 | 467.299 | 453.767 | 479.190 |
| citm_catalog.json | 903.560 | 916.633 | 800.773 | 979.304 | 931.142 | 1012.409 |
| twitter.json | 459.739 | 595.706 | 678.219 | 637.311 | 645.508 | 689.872 |
| apache_builds.json | 523.185 | 735.630 | 783.090 | 926.556 | 1019.991 | 1037.426 |
| instruments.json | 497.958 | 607.336 | 702.804 | 905.769 | 827.316 | 986.565 |
| mesh.json | 440.380 | 402.142 | 442.073 | 525.972 | 468.483 | 559.219 |
| update-center.json | 410.063 | 601.749 | 559.382 | 788.338 | 884.310 | 1010.891 |
| harvard.json | 345.587 | 434.780 | 469.948 | 623.669 | 668.160 | 741.455 |
| citylots.json | 383.896 | 349.618 | 366.420 | 421.005 | 409.356 | 437.085 |
| random_uint.json | 417.192 | 381.187 | 362.930 | 583.177 | 521.227 | 702.244 |
| random_bool.json | 242.595 | 220.516 | 332.500 | 477.118 | 386.896 | 424.262 |
| random_int.json | 264.090 | 245.817 | 250.786 | 393.685 | 398.201 | 450.163 |
