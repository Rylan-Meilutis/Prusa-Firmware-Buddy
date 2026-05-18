# Theme JSON files

USB theme import accepts one complete JSON object. The file name must end in `.json`.

The loader validates the file before saving the imported theme. It rejects malformed JSON, missing required fields, duplicate keys, unknown keys, and color values that are not strings in `#RRGGBB` or `RRGGBB` format.

Required top-level keys:

- `ui`: UI theme colors.
- `led`: status LED colors.

Optional top-level keys:

- `name`: string for your own reference.
- `schema_version`: number. Use `1`.

Required `ui` colors:

- `primary`
- `progress`
- `warning`
- `error`
- `image`: runtime tint color for theme-colored artwork. Only pixels matching the firmware's indigo theme marker are recolored; other image colors are preserved.

Required `led` colors:

- `idle`
- `printing`
- `finished`
- `warning`
- `error`

Example:

```json
{
  "name": "RME Indigo",
  "schema_version": 1,
  "ui": {
    "primary": "#4B2E83",
    "progress": "#4B2E83",
    "warning": "#FFCC00",
    "error": "#FF2F2F",
    "image": "#4B2E83"
  },
  "led": {
    "idle": "#000000",
    "printing": "#0096FF",
    "finished": "#00FF00",
    "warning": "#FFCC00",
    "error": "#FF0000"
  }
}
```
