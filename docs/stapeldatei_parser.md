# STAPELDATEI PARSER - COMPLETE ARCHITECTURE
## Autonomous Iteration 6 | Script Execution Engine

---

## 🎯 DESIGN PHILOSOPHY

**Start Simple, Grow Later:**
```
Phase 1: Minimal Parser (commands only)
  ├─ command arg1=val1 arg2=val2
  ├─ # comments
  └─ Newlines as delimiters

Phase 2: Variables
  ├─ $var = value
  └─ $var in arguments

Phase 3: Control Flow (optional!)
  ├─ if/endif
  ├─ for/endfor
  └─ while/endwhile

Phase 4: Functions (optional!)
  ├─ function name ... endfunction
  └─ Call by name
```

**For CorESP32: Phase 1-2 is SUFFICIENT for most use cases!**

---

## 📋 SYNTAX SPECIFICATION (Phase 1-2)

### Grammar (EBNF-like)

```
Script ::= Line*

Line ::= Comment | Assignment | Command | EmptyLine

Comment ::= '#' text NewLine

Assignment ::= '$' Identifier '=' Expression NewLine

Command ::= Identifier Arguments NewLine

Arguments ::= (Argument)*

Argument ::= NamedArg | PositionalArg

NamedArg ::= Identifier '=' Expression

PositionalArg ::= Expression

Expression ::= Variable | String | Number

Variable ::= '$' Identifier

String ::= QuotedString | Unquoted

Number ::= Integer | Float

Identifier ::= [a-zA-Z_][a-zA-Z0-9_]*
```

### Example Stapeldatei

```bash
# Blink LED Example
$pin = 2
$delay = 500

gpio_mode pin=$pin mode=output

gpio_set pin=$pin level=1
delay_ms $delay
gpio_set pin=$pin level=0
delay_ms $delay
```

---

## 🏗️ ARCHITECTURE

```
┌─────────────────┐
│  .csp File      │
│  (Text Input)   │
└────────┬────────┘
         │
    ┌────▼────┐
    │  LEXER  │ → Token Stream
    └────┬────┘
         │
    ┌────▼────┐
    │ PARSER  │ → AST (Abstract Syntax Tree)
    └────┬────┘
         │
    ┌────▼────────┐
    │  EXECUTOR   │ → Execute Commands
    └─────────────┘
```

---

## 🔧 PHASE 1: LEXER

### Token Types

```c
// token.h
typedef enum {
    TOKEN_EOF,         // End of file
    TOKEN_COMMENT,     // # comment
    TOKEN_NEWLINE,     // \n
    TOKEN_IDENTIFIER,  // gpio_set, $var
    TOKEN_NUMBER,      // 123, 3.14
    TOKEN_STRING,      // "text"
    TOKEN_ASSIGN,      // =
    TOKEN_DOLLAR,      // $
    TOKEN_COMMA,       // ,
    TOKEN_UNKNOWN
} token_type_t;

typedef struct {
    token_type_t type;
    char* value;         // Actual text
    int line;            // Line number (for errors)
    int column;          // Column number
} token_t;
```

### Lexer Implementation

```c
// lexer.c
typedef struct {
    const char* input;   // Input string
    size_t pos;          // Current position
    int line;            // Current line
    int column;          // Current column
} lexer_t;

void lexer_init(lexer_t* lex, const char* input) {
    lex->input = input;
    lex->pos = 0;
    lex->line = 1;
    lex->column = 1;
}

char lexer_peek(lexer_t* lex) {
    return lex->input[lex->pos];
}

char lexer_advance(lexer_t* lex) {
    char c = lex->input[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return c;
}

void lexer_skip_whitespace(lexer_t* lex) {
    while (lexer_peek(lex) == ' ' || lexer_peek(lex) == '\t') {
        lexer_advance(lex);
    }
}

token_t* lexer_next_token(lexer_t* lex) {
    lexer_skip_whitespace(lex);
    
    char c = lexer_peek(lex);
    
    // EOF
    if (c == '\0') {
        return token_new(TOKEN_EOF, "", lex->line, lex->column);
    }
    
    // Comment
    if (c == '#') {
        lexer_advance(lex);
        // Skip until newline
        while (lexer_peek(lex) != '\n' && lexer_peek(lex) != '\0') {
            lexer_advance(lex);
        }
        return token_new(TOKEN_COMMENT, "#", lex->line, lex->column);
    }
    
    // Newline
    if (c == '\n') {
        lexer_advance(lex);
        return token_new(TOKEN_NEWLINE, "\n", lex->line, lex->column);
    }
    
    // Variable
    if (c == '$') {
        lexer_advance(lex);
        // Read identifier
        char buf[256];
        int i = 0;
        while (isalnum(lexer_peek(lex)) || lexer_peek(lex) == '_') {
            buf[i++] = lexer_advance(lex);
        }
        buf[i] = '\0';
        return token_new(TOKEN_IDENTIFIER, buf, lex->line, lex->column);
    }
    
    // Assignment
    if (c == '=') {
        lexer_advance(lex);
        return token_new(TOKEN_ASSIGN, "=", lex->line, lex->column);
    }
    
    // Number
    if (isdigit(c) || (c == '-' && isdigit(lex->input[lex->pos + 1]))) {
        char buf[256];
        int i = 0;
        if (c == '-') buf[i++] = lexer_advance(lex);
        
        while (isdigit(lexer_peek(lex))) {
            buf[i++] = lexer_advance(lex);
        }
        
        // Float?
        if (lexer_peek(lex) == '.') {
            buf[i++] = lexer_advance(lex);
            while (isdigit(lexer_peek(lex))) {
                buf[i++] = lexer_advance(lex);
            }
        }
        
        buf[i] = '\0';
        return token_new(TOKEN_NUMBER, buf, lex->line, lex->column);
    }
    
    // String (quoted)
    if (c == '"') {
        lexer_advance(lex);
        char buf[1024];
        int i = 0;
        while (lexer_peek(lex) != '"' && lexer_peek(lex) != '\0') {
            buf[i++] = lexer_advance(lex);
        }
        lexer_advance(lex);  // Skip closing "
        buf[i] = '\0';
        return token_new(TOKEN_STRING, buf, lex->line, lex->column);
    }
    
    // Identifier or Unquoted String
    if (isalpha(c) || c == '_' || c == '/') {
        char buf[256];
        int i = 0;
        while (isalnum(lexer_peek(lex)) || 
               lexer_peek(lex) == '_' || 
               lexer_peek(lex) == '/' ||
               lexer_peek(lex) == '.' ||
               lexer_peek(lex) == ':') {
            buf[i++] = lexer_advance(lex);
        }
        buf[i] = '\0';
        return token_new(TOKEN_IDENTIFIER, buf, lex->line, lex->column);
    }
    
    // Unknown
    lexer_advance(lex);
    return token_new(TOKEN_UNKNOWN, "", lex->line, lex->column);
}
```

---

## 🌳 PHASE 2: PARSER (AST Generation)

### AST Node Types

```c
// ast.h
typedef enum {
    AST_COMMENT,
    AST_ASSIGNMENT,
    AST_COMMAND,
} ast_node_type_t;

typedef struct ast_node {
    ast_node_type_t type;
    union {
        struct {  // AST_ASSIGNMENT
            char* var_name;
            char* value;
        } assignment;
        
        struct {  // AST_COMMAND
            char* name;
            int arg_count;
            struct {
                char* key;    // NULL for positional
                char* value;
            } args[32];
        } command;
    };
    
    struct ast_node* next;  // Linked list
} ast_node_t;
```

### Parser Implementation

```c
// parser.c
typedef struct {
    lexer_t* lex;
    token_t* current_token;
} parser_t;

void parser_init(parser_t* p, lexer_t* lex) {
    p->lex = lex;
    p->current_token = lexer_next_token(lex);
}

void parser_advance(parser_t* p) {
    token_free(p->current_token);
    p->current_token = lexer_next_token(p->lex);
}

bool parser_expect(parser_t* p, token_type_t type) {
    if (p->current_token->type != type) {
        fprintf(stderr, "Parse error: Expected %d, got %d at line %d\n",
                type, p->current_token->type, p->current_token->line);
        return false;
    }
    return true;
}

// Parse Assignment: $var = value
ast_node_t* parser_parse_assignment(parser_t* p) {
    // $var
    if (!parser_expect(p, TOKEN_IDENTIFIER)) return NULL;
    char* var_name = strdup(p->current_token->value);
    parser_advance(p);
    
    // =
    if (!parser_expect(p, TOKEN_ASSIGN)) {
        free(var_name);
        return NULL;
    }
    parser_advance(p);
    
    // value (identifier, number, or string)
    char* value = NULL;
    if (p->current_token->type == TOKEN_IDENTIFIER ||
        p->current_token->type == TOKEN_NUMBER ||
        p->current_token->type == TOKEN_STRING) {
        value = strdup(p->current_token->value);
        parser_advance(p);
    } else {
        fprintf(stderr, "Parse error: Expected value\n");
        free(var_name);
        return NULL;
    }
    
    // Create AST node
    ast_node_t* node = calloc(1, sizeof(ast_node_t));
    node->type = AST_ASSIGNMENT;
    node->assignment.var_name = var_name;
    node->assignment.value = value;
    
    return node;
}

// Parse Command: command_name arg1=val1 arg2=val2
ast_node_t* parser_parse_command(parser_t* p) {
    // Command name
    if (!parser_expect(p, TOKEN_IDENTIFIER)) return NULL;
    char* cmd_name = strdup(p->current_token->value);
    parser_advance(p);
    
    // Arguments
    ast_node_t* node = calloc(1, sizeof(ast_node_t));
    node->type = AST_COMMAND;
    node->command.name = cmd_name;
    node->command.arg_count = 0;
    
    while (p->current_token->type == TOKEN_IDENTIFIER ||
           p->current_token->type == TOKEN_NUMBER ||
           p->current_token->type == TOKEN_STRING) {
        
        char* key = NULL;
        char* value = NULL;
        
        // Named argument: key=value
        if (p->current_token->type == TOKEN_IDENTIFIER) {
            char* first = strdup(p->current_token->value);
            parser_advance(p);
            
            if (p->current_token->type == TOKEN_ASSIGN) {
                // Named arg
                parser_advance(p);
                key = first;
                
                if (p->current_token->type == TOKEN_IDENTIFIER ||
                    p->current_token->type == TOKEN_NUMBER ||
                    p->current_token->type == TOKEN_STRING) {
                    value = strdup(p->current_token->value);
                    parser_advance(p);
                } else {
                    fprintf(stderr, "Parse error: Expected value after =\n");
                    free(key);
                    break;
                }
            } else {
                // Positional arg
                key = NULL;
                value = first;
            }
        } else {
            // Positional arg (number or string)
            value = strdup(p->current_token->value);
            parser_advance(p);
        }
        
        // Add to node
        int idx = node->command.arg_count++;
        node->command.args[idx].key = key;
        node->command.args[idx].value = value;
    }
    
    return node;
}

// Parse Script
ast_node_t* parser_parse(parser_t* p) {
    ast_node_t* root = NULL;
    ast_node_t* current = NULL;
    
    while (p->current_token->type != TOKEN_EOF) {
        ast_node_t* node = NULL;
        
        // Skip comments and empty lines
        if (p->current_token->type == TOKEN_COMMENT ||
            p->current_token->type == TOKEN_NEWLINE) {
            parser_advance(p);
            continue;
        }
        
        // Assignment: $ at start
        if (p->current_token->type == TOKEN_IDENTIFIER && 
            p->current_token->value[0] == '$') {
            node = parser_parse_assignment(p);
        }
        // Command
        else if (p->current_token->type == TOKEN_IDENTIFIER) {
            node = parser_parse_command(p);
        }
        else {
            fprintf(stderr, "Parse error: Unexpected token at line %d\n",
                    p->current_token->line);
            parser_advance(p);
            continue;
        }
        
        if (node) {
            if (!root) {
                root = node;
                current = node;
            } else {
                current->next = node;
                current = node;
            }
        }
        
        // Skip newline after statement
        if (p->current_token->type == TOKEN_NEWLINE) {
            parser_advance(p);
        }
    }
    
    return root;
}
```

---

## ⚙️ PHASE 3: EXECUTOR

### Variable Storage

```c
// executor.h
typedef struct {
    char* name;
    char* value;
} variable_t;

typedef struct {
    variable_t vars[256];
    int var_count;
} executor_ctx_t;

void executor_init(executor_ctx_t* ctx) {
    ctx->var_count = 0;
}

void executor_set_var(executor_ctx_t* ctx, const char* name, const char* value) {
    // Check if exists
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            free(ctx->vars[i].value);
            ctx->vars[i].value = strdup(value);
            return;
        }
    }
    
    // Add new
    if (ctx->var_count < 256) {
        ctx->vars[ctx->var_count].name = strdup(name);
        ctx->vars[ctx->var_count].value = strdup(value);
        ctx->var_count++;
    }
}

const char* executor_get_var(executor_ctx_t* ctx, const char* name) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->vars[i].name, name) == 0) {
            return ctx->vars[i].value;
        }
    }
    return NULL;
}
```

### Executor Implementation

```c
// executor.c
typedef esp_err_t (*command_handler_t)(int argc, char** argv);

extern command_handler_t get_command_handler(const char* name);

// Resolve Variables in Arguments
char* executor_resolve_value(executor_ctx_t* ctx, const char* value) {
    // Check if variable
    if (value[0] == '$') {
        const char* var_value = executor_get_var(ctx, value + 1);
        if (var_value) {
            return strdup(var_value);
        } else {
            fprintf(stderr, "Warning: Undefined variable %s\n", value);
            return strdup("");
        }
    } else {
        return strdup(value);
    }
}

// Execute Single Node
esp_err_t executor_execute_node(executor_ctx_t* ctx, ast_node_t* node) {
    if (node->type == AST_ASSIGNMENT) {
        // Resolve value
        char* resolved = executor_resolve_value(ctx, node->assignment.value);
        executor_set_var(ctx, node->assignment.var_name, resolved);
        free(resolved);
        return ESP_OK;
    }
    else if (node->type == AST_COMMAND) {
        // Build argv
        int argc = node->command.arg_count + 1;
        char** argv = malloc(sizeof(char*) * (argc + 1));
        
        argv[0] = node->command.name;
        
        for (int i = 0; i < node->command.arg_count; i++) {
            char arg_str[512];
            
            // Resolve value
            char* resolved = executor_resolve_value(ctx, node->command.args[i].value);
            
            if (node->command.args[i].key) {
                // Named: key=value
                snprintf(arg_str, sizeof(arg_str), "%s=%s",
                         node->command.args[i].key, resolved);
            } else {
                // Positional
                snprintf(arg_str, sizeof(arg_str), "%s", resolved);
            }
            
            argv[i + 1] = strdup(arg_str);
            free(resolved);
        }
        
        argv[argc] = NULL;
        
        // Find and execute command
        command_handler_t handler = get_command_handler(node->command.name);
        esp_err_t ret = ESP_ERR_NOT_FOUND;
        
        if (handler) {
            ret = handler(argc, argv);
        } else {
            fprintf(stderr, "Error: Unknown command '%s'\n", node->command.name);
        }
        
        // Free argv
        for (int i = 1; i < argc; i++) {
            free(argv[i]);
        }
        free(argv);
        
        return ret;
    }
    
    return ESP_OK;
}

// Execute Full AST
esp_err_t executor_execute(ast_node_t* ast) {
    executor_ctx_t ctx;
    executor_init(&ctx);
    
    ast_node_t* current = ast;
    while (current) {
        esp_err_t ret = executor_execute_node(&ctx, current);
        if (ret != ESP_OK) {
            fprintf(stderr, "Execution error: %s\n", esp_err_to_name(ret));
            return ret;
        }
        current = current->next;
    }
    
    return ESP_OK;
}
```

---

## 🎮 HIGH-LEVEL API

### Simple API for User

```c
// stapeldatei.h

/**
 * Execute stapeldatei from file
 */
esp_err_t stapeldatei_exec(const char* path) {
    // Read file
    corefs_mmap_t* mmap = corefs_mmap(path);
    if (!mmap) {
        ESP_LOGE("stapel", "Failed to open %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    const char* content = (const char*)mmap->data;
    
    // Lex
    lexer_t lex;
    lexer_init(&lex, content);
    
    // Parse
    parser_t parser;
    parser_init(&parser, &lex);
    ast_node_t* ast = parser_parse(&parser);
    
    if (!ast) {
        corefs_munmap(mmap);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Execute
    esp_err_t ret = executor_execute(ast);
    
    // Cleanup
    ast_free(ast);
    corefs_munmap(mmap);
    
    return ret;
}

/**
 * Execute stapeldatei from string
 */
esp_err_t stapeldatei_exec_string(const char* script) {
    lexer_t lex;
    lexer_init(&lex, script);
    
    parser_t parser;
    parser_init(&parser, &lex);
    ast_node_t* ast = parser_parse(&parser);
    
    if (!ast) return ESP_ERR_INVALID_ARG;
    
    esp_err_t ret = executor_execute(ast);
    ast_free(ast);
    
    return ret;
}
```

---

## 📋 NEW GRUNDBEFEHLE

### Command: `exec`

**Purpose:** Execute Stapeldatei

**Syntax:**
```bash
exec /stapel/blink.csp
```

**Implementation:**
```c
int cmd_exec(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: exec <path>\n");
        return -1;
    }
    
    const char* path = argv[1];
    
    esp_err_t ret = stapeldatei_exec(path);
    if (ret != ESP_OK) {
        printf("Failed to execute %s: %s\n", path, esp_err_to_name(ret));
        return -1;
    }
    
    return 0;
}
```

---

## 🧪 TESTING

### Test 1: Variable Assignment

```bash
# test_vars.csp
$x = 10
$y = 20
$z = $x  # Variable reference

echo $x  # Should print "10"
echo $y  # Should print "20"
echo $z  # Should print "10"
```

**Expected Output:**
```
10
20
10
```

---

### Test 2: GPIO Control

```bash
# test_gpio.csp
$pin = 2
$delay = 500

gpio_mode pin=$pin mode=output

gpio_set pin=$pin level=1
delay_ms $delay
gpio_set pin=$pin level=0
delay_ms $delay
```

**Expected:** LED on pin 2 blinks once

---

### Test 3: Complex Example

```bash
# test_complex.csp
# Display Animation

$fps = 30
$frame_time = 33  # 1000ms / 30fps

$frame_count = 10

# Loop through frames (manually, no 'for' yet!)
display_load_frame file=/anim/frame0.raw
delay_ms $frame_time

display_load_frame file=/anim/frame1.raw
delay_ms $frame_time

# ... etc for 10 frames

echo "Animation complete"
```

---

## ⚡ OPTIMIZATION

### Pre-compilation (Optional Phase 4)

**Idea:** Compile .csp → Bytecode for faster execution

**Bytecode Format:**
```
Opcode | Argument Count | Arguments...
```

**Example:**
```c
typedef enum {
    OP_SET_VAR,      // arg1=var_name, arg2=value
    OP_CALL_CMD,     // arg1=cmd_name, arg2=argc, args...
    OP_HALT
} opcode_t;

typedef struct {
    opcode_t op;
    int arg_count;
    char** args;
} bytecode_insn_t;

bytecode_insn_t* compile_ast(ast_node_t* ast) {
    // Convert AST → Bytecode
    // ...
}

esp_err_t execute_bytecode(bytecode_insn_t* code) {
    // Execute bytecode (faster than interpreting!)
    // ...
}
```

---

## 📊 PERFORMANCE

| Method | Parse Time | Execute Time | Memory | Use Case |
|--------|-----------|-------------|--------|----------|
| **Interpreted** | ~10ms | ~1ms/cmd | Low | Development, Debugging |
| **Bytecode** | ~10ms (once) | ~0.1ms/cmd | Medium | Production, Loops |
| **Native** | - | ~0.01ms/cmd | Zero | Performance-critical |

**Recommendation:** Start with Interpreted, optimize later if needed!

---

## ✅ IMPLEMENTATION CHECKLIST

```
□ Lexer implemented (tokens)
□ Parser implemented (AST)
□ Executor implemented (variables + commands)
□ High-level API (stapeldatei_exec)
□ Command: exec
□ Test 1 (Variables) passes
□ Test 2 (GPIO) passes
□ Test 3 (Complex) passes
□ Error handling (syntax errors)
□ Memory management (no leaks!)
□ Documentation
```

---

## 🎯 CONCLUSION

**Stapeldatei Parser ist FERTIG designed!**

Simple, extensible, production-ready for Phase 1-2.

**Effort:** ~3-4 Tage Implementation

**Next:** Display System, dann FINAL Integration!

---

**END OF ITERATION 6**

*[Final iterations: Display System + Integration Strategy]*
