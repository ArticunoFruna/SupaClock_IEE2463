import re

with open("src/tests/test_general.c", "r") as f:
    text = f.read()

replacement = '''static void switch_to(ui_screen_t s) {
    if (s == current_screen) return;
    
    /* Decidir dirección de animación: por defecto hacia adelante (pantalla nueva entra por la derecha -> MOVE_LEFT).
       Si retrocedemos, la pantalla entra por la izquierda -> MOVE_RIGHT. */
    lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
    
    if (s == (current_screen + SCREEN_CYCLE_COUNT - 1) % SCREEN_CYCLE_COUNT) {
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    } else if (s < current_screen && !(current_screen == SCREEN_CYCLE_COUNT - 1 && s == 0)) {
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    }
    
    current_screen = s;
    /* Animación de 150 ms para que el deslizamiento se vea fluido. */
    lv_scr_load_anim(scr_obj[s], anim, 150, 0, false);'''

text = re.sub(
    r'static void switch_to\(ui_screen_t s\) \{\n    current_screen = s;\n    /\* Cross-fade.*?\n    lv_scr_load_anim\(scr_obj\[s\], LV_SCR_LOAD_ANIM_FADE_ON, 80, 0, false\);',
    replacement,
    text
)

with open("src/tests/test_general.c", "w") as f:
    f.write(text)
