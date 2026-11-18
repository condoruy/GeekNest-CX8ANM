# Muestra y guarda en registro.txt lo que la radio informa por USB.
import sys, time, serial
s = serial.Serial()
s.port = sys.argv[1]; s.baudrate = 115200; s.timeout = 0.5
s.dtr = False; s.rts = False  # no reiniciar la radio al abrir el puerto
s.open()
with open("registro.txt", "a", encoding="utf-8", errors="replace") as f:
    f.write("\n===== %s =====\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
    try:
        while True:
            line = s.readline()
            if line:
                text = time.strftime("%H:%M:%S ") + line.decode("utf-8", "replace").rstrip()
                print(text); f.write(text + "\n"); f.flush()
    except KeyboardInterrupt:
        pass
