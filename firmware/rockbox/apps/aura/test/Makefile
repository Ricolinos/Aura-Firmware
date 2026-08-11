# Tests host-side de la logica pura de Aura UI (sin Rockbox).
# Uso: make -C apps/aura/test

CC ?= cc
CFLAGS := -std=c99 -Wall -Wextra -Werror -O0 -g

BUILD := build

.PHONY: test clean

test: $(BUILD)/test_nav $(BUILD)/test_lrc $(BUILD)/test_splash_lang $(BUILD)/test_motion $(BUILD)/test_wheel
	@echo "== test_nav =="
	@$(BUILD)/test_nav
	@echo "== test_lrc =="
	@$(BUILD)/test_lrc
	@echo "== test_splash_lang =="
	@$(BUILD)/test_splash_lang
	@echo "== test_motion =="
	@$(BUILD)/test_motion
	@echo "== test_wheel =="
	@$(BUILD)/test_wheel

$(BUILD)/test_nav: test_nav.c ../aura_nav.c ../aura_nav.h | $(BUILD)
	$(CC) $(CFLAGS) test_nav.c ../aura_nav.c -o $@

$(BUILD)/test_motion: test_motion.c ../aura_motion.c ../aura_motion.h | $(BUILD)
	$(CC) $(CFLAGS) test_motion.c ../aura_motion.c -o $@

$(BUILD)/test_wheel: test_wheel.c ../aura_wheel.c ../aura_wheel.h | $(BUILD)
	$(CC) $(CFLAGS) test_wheel.c ../aura_wheel.c -o $@

$(BUILD)/test_lrc: test_lrc.c ../aura_lrc.c ../aura_lrc.h | $(BUILD)
	$(CC) $(CFLAGS) test_lrc.c ../aura_lrc.c -o $@

$(BUILD)/test_splash_lang: test_splash_lang.c ../aura_splash_lang.c ../aura_splash_lang.h ../aura_settings.h | $(BUILD)
	$(CC) $(CFLAGS) test_splash_lang.c ../aura_splash_lang.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
