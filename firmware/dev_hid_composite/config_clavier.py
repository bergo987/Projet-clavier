import serial
import time

PORT = 'COM5'

# Mapping complet - modifie ces valeurs selon tes besoins
# Format : HID keycodes (voir usb_hid_keys)
HID_KEY_A = 0x09
HID_KEY_B = 0x05
HID_KEY_C = 0x06
HID_KEY_D = 0x07
HID_KEY_E = 0x08
HID_KEY_F = 0x09
HID_KEY_G = 0x0A
HID_KEY_H = 0x0B
HID_KEY_I = 0x0C
HID_KEY_J = 0x0D
HID_KEY_K = 0x0E
HID_KEY_L = 0x0F
HID_KEY_M = 0x10
HID_KEY_N = 0x11
HID_KEY_O = 0x12
HID_KEY_P = 0x13
HID_KEY_Q = 0x14
HID_KEY_R = 0x15
HID_KEY_S = 0x16
HID_KEY_T = 0x17
HID_KEY_SHIFT_LEFT    = 0xE1
HID_KEY_CONTROL_RIGHT = 0xE4

# Ta matrice 5 lignes x 4 colonnes (même ordre que dans le firmware)
key_matrix = [
    [HID_KEY_S,           HID_KEY_S,           HID_KEY_S, HID_KEY_S],
    [HID_KEY_S,           HID_KEY_S,           HID_KEY_S, HID_KEY_S],
    [HID_KEY_S,           HID_KEY_S,           HID_KEY_K, HID_KEY_S],
    [HID_KEY_S,           HID_KEY_S,           HID_KEY_S, HID_KEY_S],
    [HID_KEY_S,           HID_KEY_S,           HID_KEY_S, HID_KEY_S],
]

def send_config():
    # Aplatir la matrice en 20 octets
    data = []
    for row in key_matrix:
        for key in row:
            data.append(key)
    
    assert len(data) == 20, f"La matrice doit faire exactement 20 octets, got {len(data)}"
    
    try:
        print(f"Ouverture de {PORT}...")
        
        # DTR=False à l'ouverture pour éviter le reset du Pico
        ser = serial.Serial(
            PORT,
            115200,
            timeout=2,
            write_timeout=2,
            dsrdtr=False,
            rtscts=False
        )
        
        # Activation DTR APRES ouverture - c'est ça qui débloque le CDC TinyUSB
        ser.dtr = True
        time.sleep(1.5)  # Laisser le temps au Pico de monter le CDC
        
        # Vider les buffers au cas où
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        print(f"Envoi de {len(data)} octets : {[hex(b) for b in data]}")
        
        bytes_written = ser.write(bytes(data))
        ser.flush()
        
        print(f"Octets envoyés : {bytes_written}")
        
        # Attendre un éventuel retour du Pico
        time.sleep(1.0)  # augmente à 1s
        if ser.in_waiting:
            response = ser.read(ser.in_waiting)
            print(f"Réponse Pico : {response.decode()}")
        else:
            print("Pas de réponse - le Pico n'a pas reçu les 20 octets")

        time.sleep(2.0)
        while ser.in_waiting:
            response = ser.read(ser.in_waiting)
            print(f"Debug : {response.decode()}")
            time.sleep(0.1)

        print("Succès ! Mapping envoyé au clavier.")
        ser.close()
        
    except serial.SerialTimeoutException:
        print("ERREUR : Timeout - le Pico n'accepte pas les données.")
        print("→ Vérifie que le port COM est bien celui du Pico (Gestionnaire de périphériques)")
        print("→ Essaie de débrancher/rebrancher le Pico")
    except serial.SerialException as e:
        print(f"ERREUR Série : {e}")
        print("→ Le port est peut-être déjà ouvert par un autre programme (Arduino IDE, PuTTY...)")
    except Exception as e:
        print(f"ERREUR Critique : {e}")

if __name__ == "__main__":
    send_config()

    