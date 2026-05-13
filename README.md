# Scheduling-Ships
Christian Navarro, Jorge Gutiérrez, Mauricio Luna

## Configuracion por archivo

El proyecto ahora puede leer `config.txt` al iniciar. Si el archivo no existe,
usa valores por defecto.

Ejemplo:

```txt
scheduler=rr
channel_type=letrero
channel_length=10
boat_speed_ms=150
ready_queue_ordered_count=4
sign_change_time=5
equity_w=2
quantum=2
initial_left_ships=2
initial_right_ships=2
enable_keyboard_input=1
```

Valores permitidos:

- `scheduler`: `rr`, `priority`, `sjf`, `strn`, `fcfs`, `edf`
- `channel_type`: `equidad`, `letrero`, `tico`

Notas:

- `sign_change_time` solo se usa cuando `channel_type=letrero`.
- `equity_w` solo se usa cuando `channel_type=equidad`.
- `quantum` se usa en calendarizadores expropiativos como `rr` y `strn`.
- `boat_speed_ms` controla el tiempo entre ticks del canal; menor valor significa animacion mas rapida.
- En ESP32, si se ejecuta desde firmware real, `config.txt` debe estar disponible en el sistema de archivos montado o ajustarse la ruta en `CONFIG_FILE_PATH`.
