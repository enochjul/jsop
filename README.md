# jsop
JSON compatible parser

Additional features not in JSON:
* Ignore comments
* Trailing comma before `]` or `}`
* Infinity/nan literals
* Hexadecimal floating point numbers
* Unquoted object key (partial support)

Sample usage for parsing a null-terminated string:

	JsopParser<> parser;
	JsopDocument doc;
	
	if (parser.start()) {
		if (!parser.parse(str)) {
			return false;
		}
		return parser.finish(&doc);
	}

If the string is not null-terminated, then pass a second argument that points to the end of the string. You can also call parse() multiple times if you are reading a large file into a fixed sized buffer.

Compiled on 64-bit Linux with gcc 6.2.1
