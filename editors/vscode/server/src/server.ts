import {
	createConnection,
	TextDocuments,
	ProposedFeatures,
	InitializeParams,
	CompletionItem,
	CompletionItemKind,
	TextDocumentPositionParams,
	TextDocumentSyncKind,
	InitializeResult,
} from 'vscode-languageserver/node';

import {
	TextDocument
} from 'vscode-languageserver-textdocument';

// Create a connection for the server, using Node's IPC as a transport.
// Also include all preview / proposed LSP features.
const connection = createConnection(ProposedFeatures.all);

// Create a simple text document manager.
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

connection.onInitialize((params: InitializeParams) => {
	const capabilities = params.capabilities;

    const result: InitializeResult = {
		capabilities: {
			textDocumentSync: TextDocumentSyncKind.Incremental,
			completionProvider: {
				resolveProvider: true
			}
		}
	};

    return result;
});

function autoCompleteKeyword(label : string) {
	return {
		label: label,
		kind: CompletionItemKind.Text,
		data: label
	};
}

// This handler provides the initial list of the completion items.
connection.onCompletion((_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
		return [
			// Permitives types
			autoCompleteKeyword('int1'),
			autoCompleteKeyword('bool'),

			autoCompleteKeyword('int8'),
			autoCompleteKeyword('char'),

			autoCompleteKeyword('int16'),
			autoCompleteKeyword('int32'),
			autoCompleteKeyword('int64'),

			autoCompleteKeyword('uint8'),
			autoCompleteKeyword('uchar'),
			autoCompleteKeyword('uint16'),
			autoCompleteKeyword('uint32'),
			autoCompleteKeyword('uint64'),

			autoCompleteKeyword('float32'),
			autoCompleteKeyword('float64'),

			// Values
			autoCompleteKeyword('true'),
			autoCompleteKeyword('false'),
			autoCompleteKeyword('null'),

			// Keywords
			autoCompleteKeyword('load'),
			autoCompleteKeyword('import'),
			autoCompleteKeyword('void'),
			autoCompleteKeyword('varargs'),
			autoCompleteKeyword('enum'),
			autoCompleteKeyword('packed'),
			autoCompleteKeyword('struct'),
			autoCompleteKeyword('fun'),
			autoCompleteKeyword('extern'),
			autoCompleteKeyword('defer'),
			autoCompleteKeyword('if'),
			autoCompleteKeyword('else'),
			autoCompleteKeyword('for'),
			autoCompleteKeyword('while'),
			autoCompleteKeyword('switch'),
			autoCompleteKeyword('cast'),
			autoCompleteKeyword('infix'),
			autoCompleteKeyword('prefix'),
			autoCompleteKeyword('postfix'),
			autoCompleteKeyword('continue'),
			autoCompleteKeyword('break'),
			autoCompleteKeyword('return'),
			autoCompleteKeyword('type_size'),
			autoCompleteKeyword('value_size'),
		];
	}
);

// This handler resolves additional information for the item selected in
// the completion list.
connection.onCompletionResolve(
	(item: CompletionItem): CompletionItem => {
		if (item.data === 'int1' || item.data === 'bool') {
			item.detail = 'Amun integer type with 1 bit size';
			item.documentation = 'Contains a integer value that can be 1 or 0';
		}
		
		else if (item.data === 'int8' || item.data === 'char') {
			item.detail = 'Amun integer type with 8 bits size';
			item.documentation = 'Contains a singed integer value with size 8 bits';
		}
		
		else if (item.data === 'int16') {
			item.detail = 'Amun integer type with 16 bits size';
			item.documentation = 'Contains a singed integer value with size 16 bits';
		}

		else if (item.data === 'int32') {
			item.detail = 'Amun integer type with 32 bits size';
			item.documentation = 'Contains a singed integer value with size 32 bits';
		}

		else if (item.data === 'int64') {
			item.detail = 'Amun integer type with 64 bits size';
			item.documentation = 'Contains a singed integer value with size 64 bits';
		}

		else if (item.data === 'uint8' || item.data === 'uchar') {
			item.detail = 'Amun integer type with 8 bits size';
			item.documentation = 'Contains singed integer value with size 8 bits';
		}
		
		else if (item.data === 'uint16') {
			item.detail = 'Amun integer type with 16 bits size';
			item.documentation = 'Contains un singed integer value with size 16 bits';
		}

		else if (item.data === 'uint32') {
			item.detail = 'Amun integer type with 32 bits size';
			item.documentation = 'Contains un singed integer value with size 32 bits';
		}

		else if (item.data === 'uint64') {
			item.detail = 'Amun integer type with 64 bits size';
			item.documentation = 'Contains un singed integer value with size 64 bits';
		}

		else if (item.data === 'float32') {
			item.detail = 'Amun float type with 32 bits size';
			item.documentation = 'Contains floating point value with size 32 bits';
		}

		else if (item.data === 'float64') {
			item.detail = 'Amun float type with 64 bits size';
			item.documentation = 'Contains floating point value with size 64 bits';
		}

		else if (item.data === 'load') {
			item.detail = 'load statement';
			item.documentation = 'Used to load another source file';
		}

		else if (item.data === 'import') {
			item.detail = 'import statement';
			item.documentation = 'Used to import standard library file';
		}

		else if (item.data === 'enum') {
			item.detail = 'Enumeration';
			item.documentation = 'Keyword used to declare enum type';
		}

		else if (item.data === 'struct') {
			item.detail = 'Structure';
			item.documentation = 'Keyword used to declare structure type';
		}

		else if (item.data === 'packed') {
			item.detail = 'import statement';
			item.documentation = 'Keyword used to mark struct as packed structure';
		}

		else if (item.data === 'fun') {
			item.detail = 'Function keyword';
			item.documentation = 'Keyword used to declare function';
		}

		else if (item.data === 'extern') {
			item.detail = 'External keyword';
			item.documentation = 'Keyword used to mark function as external';
		}

		else if (item.data === 'intrinsic') {
			item.detail = 'intrinsic keyword';
			item.documentation = 'Keyword used to mark function as intrinsic';
		}

		else if (item.data === 'defer') {
			item.detail = 'Defer keyword';
			item.documentation = 'Keyword used to defer the function call execution until the end of scope';
		}

		else if (item.data === 'prefix') {
			item.detail = 'Prefix keyword';
			item.documentation = 'Keyword used to mark function to used as prefix operator';
		}

		else if (item.data === 'infix') {
			item.detail = 'Infix keyword';
			item.documentation = 'Keyword used to mark function to used as infix operator';
		}

		else if (item.data === 'postfix') {
			item.detail = 'Postfix keyword';
			item.documentation = 'Keyword used to mark function to used as postfix operator';
		}

		else if (item.data === 'type_size') {
			item.detail = 'Get The size type';
			item.documentation = 'Return type size at compile time as integer';
		}

		else if (item.data === 'value_size') {
			item.detail = 'Get the value size';
			item.documentation = 'Return value size at compile time as integer';
		}

		return item;
	}
);


// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();