# Tests host-side de la logica pura de Aura UI (sin Rockbox).
# Uso: make -C apps/aura/test

CC ?= cc
CFLAGS := -std=c99 -Wall -Wextra -Werror -O0 -g

BUILD := build

.PHONY: test clean

test: $(BUILD)/test_nav $(BUILD)/test_lrc $(BUILD)/test_splash_lang $(BUILD)/test_motion $(BUILD)/test_wheel $(BUILD)/test_flow $(BUILD)/test_color $(BUILD)/test_patterns $(BUILD)/test_style $(BUILD)/test_sync_marker $(BUILD)/test_device_name
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
	@echo "== test_flow =="
	@$(BUILD)/test_flow
	@echo "== test_color =="
	@$(BUILD)/test_color
	@echo "== test_patterns =="
	@$(BUILD)/test_patterns
	@echo "== test_style =="
	@$(BUILD)/test_style
	@echo "== test_sync_marker =="
	@$(BUILD)/test_sync_marker
	@echo "== test_device_name =="
	@$(BUILD)/test_device_name

$(BUILD)/test_nav: test_nav.c ../aura_nav.c ../aura_nav.h | $(BUILD)
	$(CC) $(CFLAGS) test_nav.c ../aura_nav.c -o $@

$(BUILD)/test_motion: test_motion.c ../aura_motion.c ../aura_motion.h | $(BUILD)
	$(CC) $(CFLAGS) test_motion.c ../aura_motion.c -o $@

$(BUILD)/test_wheel: test_wheel.c ../aura_wheel.c ../aura_wheel.h | $(BUILD)
	$(CC) $(CFLAGS) test_wheel.c ../aura_wheel.c -o $@

$(BUILD)/test_flow: test_flow.c ../aura_flow.c ../aura_flow.h | $(BUILD)
	$(CC) $(CFLAGS) test_flow.c ../aura_flow.c -o $@

$(BUILD)/test_color: test_color.c ../aura_color.c ../aura_color.h | $(BUILD)
	$(CC) $(CFLAGS) test_color.c ../aura_color.c -o $@

$(BUILD)/test_patterns: test_patterns.c ../aura_patterns.c ../aura_patterns.h | $(BUILD)
	$(CC) $(CFLAGS) test_patterns.c ../aura_patterns.c -o $@

$(BUILD)/test_lrc: test_lrc.c ../aura_lrc.c ../aura_lrc.h | $(BUILD)
	$(CC) $(CFLAGS) test_lrc.c ../aura_lrc.c -o $@

$(BUILD)/test_splash_lang: test_splash_lang.c ../aura_splash_lang.c ../aura_splash_lang.h ../aura_settings.h | $(BUILD)
	$(CC) $(CFLAGS) test_splash_lang.c ../aura_splash_lang.c -o $@

$(BUILD)/test_style: test_style.c ../aura_style_manifest.c ../aura_style_manifest.h | $(BUILD)
	$(CC) $(CFLAGS) test_style.c ../aura_style_manifest.c -o $@

$(BUILD)/test_sync_marker: test_sync_marker.c ../aura_sync_marker.c ../aura_sync_marker.h | $(BUILD)
	$(CC) $(CFLAGS) test_sync_marker.c ../aura_sync_marker.c -o $@

$(BUILD)/test_device_name: test_device_name.c ../aura_device_name.c ../aura_device_name.h | $(BUILD)
	$(CC) $(CFLAGS) test_device_name.c ../aura_device_name.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
