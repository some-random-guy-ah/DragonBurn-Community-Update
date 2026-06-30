#!/bin/bash

OUTPUT="project_dump.txt"
IGNORE="cmake*|fonts|icons|.idea|.git|dmp.sh|$OUTPUT"

echo "Project is cs2 external read-only cheat with features: ESP, radar, spectator list, config saving/loading, imgui menu, crosshair, parsing offsets in format of cs2 dumper, chams like ESP etc" > "$OUTPUT"
echo "" >> "$OUTPUT"
echo "Project structure:" >> "$OUTPUT"
tree -I "$IGNORE" >> "$OUTPUT"

echo "" >> "$OUTPUT"
echo "Now I'll list the project files:" >> "$OUTPUT"
echo "" >> "$OUTPUT"

tree -fi -I "$IGNORE" | grep -E '\.(cpp|h)$|CMakeLists.txt$' | while read -r f; do
    echo "FILE: $f" >> "$OUTPUT"
    cat "$f" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
done

echo "Export written to $OUTPUT"