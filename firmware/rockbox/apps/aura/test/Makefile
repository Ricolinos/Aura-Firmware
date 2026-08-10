# Tests host-side de la logica pura de Aura UI (sin Rockbox).
# Uso: make -C apps/aura/test

CC ?= cc
CFLAGS := -std=c99 -Wall -Wextra -Werror -O0 -g

BUILD := build

.PHONY: test clean

test: $(BUILD)/test_nav $(BUILD)/test_lrc
	@echo "== test_nav =="
	@$(BUILD)/test_nav
	@echo "== test_lrc =="
	@$(BUILD)/test_lrc

$(BUILD)/test_nav: test_nav.c ../aura_nav.c ../aura_nav.h | $(BUILD)
	$(CC) $(CFLAGS) test_nav.c ../aura_nav.c -o $@

$(BUILD)/test_lrc: test_lrc.c ../aura_lrc.c ../aura_lrc.h | $(BUILD)
	$(CC) $(CFLAGS) test_lrc.c ../aura_lrc.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
