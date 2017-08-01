# jsop
JSON compatible parser

Additional features not in JSON:
* Ignore comments
* Trailing comma before `]` or `}`
* Infinity/nan literals
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
| canada.json | 4.347 | 4.804 | 4.809 | 4.733 | 4.825 | 4.978 |
| citm_catalog.json | 1.828 | 1.827 | 2.071 | 2.086 | 2.038 | 1.875 |
| twitter.json | 1.306 | 1.007 | 0.879 | 0.941 | 0.957 | 0.881 |
| apache_builds.json | 0.232 | 0.198 | 0.155 | 0.130 | 0.124 | 0.113 |
| instruments.json | 0.423 | 0.437 | 0.298 | 0.261 | 0.272 | 0.227 |
| mesh.json | 1.559 | 1.727 | 1.541 | 1.433 | 1.527 | 1.369 |
| update-center.json | 1.239 | 0.837 | 0.905 | 0.603 | 0.547 | 0.488 |
| harvard.json | 268.750 | 213.498 | 197.383 | 140.137 | 133.158 | 124.426 |
| citylots.json | 471.995 | 517.291 | 493.715 | 431.577 | 453.012 | 455.435 |
| random_uint.json | 244.532 | 267.970 | 281.297 | 214.527 | 236.672 | 190.780 |
| random_bool.json | 345.738 | 381.040 | 252.719 | 178.429 | 217.854 | 199.528 |
| random_int.json | 368.175 | 395.348 | 387.624 | 294.623 | 295.819 | 267.898 |

| Bandwidth (MB/s) | RapidJSON | RapidJSON_Insitu | sajson | jsop packed 32-bit | jsop packed 64-bit | jsop |
| --- | --- | --- | --- | --- | --- | --- |
| canada.json | 493.851 | 446.871 | 446.407 | 453.575 | 444.926 | 431.251 |
| citm_catalog.json | 901.089 | 901.582 | 795.360 | 789.641 | 808.239 | 878.501 |
| twitter.json | 461.148 | 598.072 | 685.163 | 640.020 | 629.319 | 683.608 |
| apache_builds.json | 523.185 | 613.025 | 783.090 | 933.684 | 978.862 | 1074.150 |
| instruments.json | 496.781 | 480.866 | 705.162 | 805.128 | 772.567 | 925.719 |
| mesh.json | 442.640 | 399.581 | 447.810 | 481.560 | 451.916 | 504.073 |
| update-center.json | 410.394 | 607.501 | 561.854 | 843.247 | 929.576 | 1041.963 |
| harvard.json | 345.133 | 434.452 | 469.922 | 661.885 | 696.575 | 745.460 |
| citylots.json | 383.450 | 349.874 | 366.581 | 419.361 | 399.518 | 397.393 |
| random_uint.json | 417.245 | 380.750 | 362.712 | 475.603 | 431.102 | 534.803 |
| random_bool.json | 242.732 | 220.244 | 332.075 | 470.336 | 385.220 | 420.601 |
| random_int.json | 263.833 | 245.699 | 250.595 | 329.698 | 328.365 | 362.588 |
