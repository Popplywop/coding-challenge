CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -I. -g -fsanitize=address,undefined
LDFLAGS = -fsanitize=address,undefined

# Executables
TEST_EXE = run_tests
JSON_DB = compile_commands.json

# Main Targets
.PHONY: all test clean compile_commands

all: $(TEST_EXE) compile_commands

$(TEST_EXE): test_runner.c json_parser.h
	$(CC) $(CFLAGS) test_runner.c -o $@ $(LDFLAGS)

test: $(TEST_EXE)
	./$(TEST_EXE)

compile_commands:
	@echo "Generating $(JSON_DB) for clangd..."
	@echo "[" > $(JSON_DB)
	@echo "  {" >> $(JSON_DB)
	@echo "    \"directory\": \"$(CURDIR)\"," >> $(JSON_DB)
	@echo "    \"arguments\": [\"$(CC)\", \"-Wall\", \"-Wextra\", \"-Wpedantic\", \"-std=c99\", \"-I.\", \"-g\", \"-fsanitize=address,undefined\", \"test_runner.c\"]," >> $(JSON_DB)
	@echo "    \"file\": \"test_runner.c\"" >> $(JSON_DB)
	@echo "  }," >> $(JSON_DB)
	@echo "  {" >> $(JSON_DB)
	@echo "    \"directory\": \"$(CURDIR)\"," >> $(JSON_DB)
	@echo "    \"arguments\": [\"$(CC)\", \"-Wall\", \"-Wextra\", \"-Wpedantic\", \"-std=c99\", \"-I.\", \"-g\", \"-DJSON_PARSER_IMPLEMENTATION\", \"-fsyntax-only\", \"json_parser.h\"]," >> $(JSON_DB)
	@echo "    \"file\": \"json_parser.h\"" >> $(JSON_DB)
	@echo "  }" >> $(JSON_DB)
	@echo "]" >> $(JSON_DB)

clean:
	rm -f $(TEST_EXE) $(JSON_DB)
