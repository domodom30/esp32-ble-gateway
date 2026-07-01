#!/usr/bin/env python3
"""Génère src/web_ui.h à partir de data/index.html.gz.

À lancer après chaque build de la WebUI (qui produit data/index.html.gz) :
    python3 tools/gen_web_ui.py

L'UI est ainsi embarquée dans le firmware et servie depuis la flash mémoire-
mappée (cache/XIP), et non via SPIFFS. Cela évite le deadlock spi_flash_read
inter-cœurs qui figeait la tâche loop() en servant la page d'accueil après un
reboot OTA, et fait que l'OTA met à jour l'UI en même temps que le firmware.
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "data", "index.html.gz")
DST = os.path.join(ROOT, "src", "web_ui.h")


def main() -> None:
    with open(SRC, "rb") as f:
        data = f.read()

    lines = [
        "// Auto-généré depuis data/index.html.gz — NE PAS éditer à la main.",
        "// Régénérer après un build WebUI : python3 tools/gen_web_ui.py",
        "#ifndef ESP_GW_WEB_UI_H",
        "#define ESP_GW_WEB_UI_H",
        "#include <stdint.h>",
        "",
        "static const uint8_t INDEX_HTML_GZ[] = {",
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("  " + "".join("0x%02x," % b for b in chunk))
    lines.append("};")
    lines.append("")
    lines.append("static const unsigned INDEX_HTML_GZ_LEN = %d;" % len(data))
    lines.append("")
    lines.append("#endif")

    with open(DST, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("written %s  bytes=%d" % (DST, len(data)))


if __name__ == "__main__":
    main()
