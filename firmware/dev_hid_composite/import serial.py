import serial
import time

# Remplace par ton port réel (ex: 'COM3' ou '/dev/ttyACM0')
PORT = 'COM3' 

# Définition des codes HID (extraits de usb_descriptors.h)
HID_A = 0x04
HID_B = 0x05
HID_ENTER = 0x28
HID_SPACE = 0x2C
# ... ajoute ceux dont tu as besoin

# Ta nouvelle matrice 5 lignes x 4 colonnes (20 octets au total)
# Ici on fait un exemple simple :
new_matrix = [
    HID_A,     HID_B,     0x06,      0x07,  # Ligne 0
    0x08,      0x09,      0x0A,      0x0B,  # Ligne 1
    0x0C,      0x0D,      0x0E,      0x0F,  # Ligne 2
    0x10,      0x11,      0x12,      0x13,  # Ligne 3
    HID_ENTER, HID_SPACE, 0x16,      0x17   # Ligne 4
]

def update_keyboard():
    try:
        # Ouverture du port série
        ser = serial.Serial(PORT, 115200, timeout=1)
        time.sleep(1) # Petit temps d'attente pour l'init
        
        print(f"Envoi de la nouvelle matrice vers {PORT}...")
        
        # On envoie les 20 octets bruts
        ser.write(bytes(new_matrix))
        
        print("Données envoyées ! Regarde ton écran OLED.")
        ser.close()
    except Exception as e:
        print(f"Erreur : {e}")

if __name__ == "__main__":
    update_keyboard()