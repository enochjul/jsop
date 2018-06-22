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
Compiled on Pentium G3258 for x86-64 with gcc 8.1.1:

	g++ -O2 -march=native -fno-exceptions -fno-rtti -DNDEBUG

| Time (ms) | RapidJSON | RapidJSON_Insitu | sajson | jsop packed 32-bit | jsop packed 64-bit | jsop |
| --- | --- | --- | --- | --- | --- | --- |
| canada.json | 4.337 | 4.710 | 4.853 | 3.629 | 3.632 | 4.159 |
| citm_catalog.json | 1.824 | 1.806 | 2.068 | 1.856 | 1.772 | 1.766 |
| twitter.json | 1.291 | 1.012 | 0.918 | 0.976 | 1.013 | 0.924 |
| apache_builds.json | 0.231 | 0.164 | 0.160 | 0.124 | 0.123 | 0.129 |
| instruments.json | 0.428 | 0.350 | 0.304 | 0.253 | 0.271 | 0.243 |
| mesh.json | 1.604 | 1.716 | 1.604 | 1.272 | 1.365 | 1.221 |
| update-center.json | 1.221 | 0.854 | 0.960 | 0.645 | 0.603 | 0.551 |
| harvard.json | 273.482 | 224.912 | 210.081 | 154.635 | 147.562 | 138.999 |
| citylots.json | 479.360 | 534.392 | 523.090 | 376.453 | 382.291 | 379.701 |
| random_uint.json | 254.322 | 299.601 | 292.233 | 152.880 | 176.901 | 129.933 |
| random_bool.json | 369.858 | 407.020 | 266.790 | 182.266 | 228.147 | 207.902 |
| random_int.json | 372.545 | 421.188 | 408.697 | 244.571 | 248.304 | 217.914 |

| Bandwidth (MB/s) | RapidJSON | RapidJSON_Insitu | sajson | jsop packed 32-bit | jsop packed 64-bit | jsop |
| --- | --- | --- | --- | --- | --- | --- |
| canada.json | 494.990 | 455.790 | 442.359 | 591.560 | 591.071 | 516.174 |
| citm_catalog.json | 903.065 | 912.065 | 796.514 | 887.495 | 929.566 | 932.724 |
| twitter.json | 466.506 | 595.117 | 656.055 | 617.068 | 594.530 | 651.795 |
| apache_builds.json | 525.450 | 740.115 | 758.618 | 978.862 | 986.820 | 940.922 |
| instruments.json | 490.977 | 600.395 | 691.244 | 830.586 | 775.418 | 864.767 |
| mesh.json | 430.222 | 402.142 | 430.222 | 542.512 | 505.550 | 565.173 |
| update-center.json | 416.444 | 595.408 | 529.665 | 788.338 | 843.247 | 922.828 |
| harvard.json | 339.162 | 412.404 | 441.518 | 599.829 | 628.581 | 667.304 |
| citylots.json | 377.559 | 338.678 | 345.995 | 480.768 | 473.426 | 476.656 |
| random_uint.json | 401.183 | 340.552 | 349.138 | 667.384 | 576.762 | 785.249 |
| random_bool.json | 226.902 | 206.186 | 314.561 | 460.435 | 367.840 | 403.660 |
| random_int.json | 260.738 | 230.626 | 237.674 | 397.172 | 391.201 | 445.757 |
