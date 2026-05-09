import tkinter as tk
from tkinter import messagebox, ttk, colorchooser
import serial
import time
import colorsys

# --- CONFIGURATION ---
PORT = 'COM5'
HID_KEYS = {
    "A": 0x04, "B": 0x05, "C": 0x06, "D": 0x07, "E": 0x08, "F": 0x09,
    "G": 0x0A, "H": 0x0B, "I": 0x0C, "J": 0x0D, "K": 0x0E, "L": 0x0F,
    "M": 0x10, "N": 0x11, "O": 0x12, "P": 0x13, "Q": 0x14, "R": 0x15,
    "S": 0x16, "T": 0x17, "U": 0x18, "V": 0x19, "W": 0x1A, "X": 0x1B,
    "Y": 0x1C, "Z": 0x1D, "ENTER": 0x28, "ESCAPE": 0x29, "BACKSPACE": 0x2A,
    "TAB": 0x2B, "SPACE": 0x2C, "SHIFT_L": 0xE1, "CTRL_R": 0xE4, "ALT_L": 0xE2
}

class KeyboardConfigurator:
    def __init__(self, root):
        self.root = root
        self.root.title("Pico Master Configurator")
        self.root.geometry("700x750")
        
        # --- Zone Port COM ---
        port_frame = tk.LabelFrame(root, text=" Connexion ", padx=10, pady=10)
        port_frame.pack(pady=10)
        tk.Label(port_frame, text="Port :").pack(side=tk.LEFT)
        self.port_entry = tk.Entry(port_frame, width=10)
        self.port_entry.insert(0, PORT)
        self.port_entry.pack(side=tk.LEFT, padx=5)

        # --- Zone Matrice des Touches ---
        tk.Label(root, text="MAPPING DES TOUCHES", font=('Arial', 10, 'bold')).pack()
        self.key_frame = tk.Frame(root)
        self.key_frame.pack(pady=10)
        
        self.key_vars = []
        for r in range(5):
            row_vars = []
            for c in range(4):
                var = tk.StringVar(root)
                var.set("S") # Touche par défaut
                cb = ttk.Combobox(self.key_frame, textvariable=var, values=list(HID_KEYS.keys()), width=7)
                cb.grid(row=r, column=c, padx=3, pady=3)
                row_vars.append(var)
            self.key_vars.append(row_vars)

        # --- Zone Matrice des LEDs ---
        tk.Label(root, text="COULEURS DES LEDS (Per-Key)", font=('Arial', 10, 'bold')).pack(pady=(10, 0))
        self.led_frame = tk.Frame(root)
        self.led_frame.pack(pady=10)
        
        self.led_hues = [0] * 20 # Stocke les teintes (Hues)
        self.led_buttons = []
        
        for i in range(20):
            btn = tk.Button(self.led_frame, text=f"L{i+1}", width=6, height=2, 
                            bg="black", fg="white", command=lambda idx=i: self.pick_color(idx))
            btn.grid(row=i // 4, column=i % 4, padx=3, pady=3)
            self.led_buttons.append(btn)

        # --- Bouton d'envoi ---
        self.send_btn = tk.Button(root, text="FLASHER LA CONFIG (40 Octets)", command=self.send_all, 
                                  bg="#2196F3", fg="white", font=('Arial', 12, 'bold'), height=2, width=30)
        self.send_btn.pack(pady=20)

    def pick_color(self, idx):
        color = colorchooser.askcolor(title=f"Couleur pour LED {idx+1}")
        if color[1]: # Si l'utilisateur n'a pas annulé
            rgb = color[0] # (R, G, B) de 0 à 255
            # Conversion RGB -> HSV pour extraire la teinte
            h, s, v = colorsys.rgb_to_hsv(rgb[0]/255.0, rgb[1]/255.0, rgb[2]/255.0)
            self.led_hues[idx] = int(h * 255)
            self.led_buttons[idx].config(bg=color[1])

    def send_all(self):
        port = self.port_entry.get()
        payload = []
        
        try:
            # 1. Collecte des touches (20 octets)
            for row in self.key_vars:
                for var in row:
                    payload.append(HID_KEYS[var.get()])
            
            # 2. Ajout des teintes (20 octets)
            payload.extend(self.led_hues)
            
            if len(payload) != 40:
                raise ValueError(f"Payload incorrect: {len(payload)} octets")

            print(f"Envoi vers {port}...")
            ser = serial.Serial(port, 115200, timeout=2, dsrdtr=False)
            ser.dtr = True
            time.sleep(1.5)
            
            ser.write(bytes(payload))
            ser.flush()
            
            # Lecture du debug renvoyé par le Pico
            time.sleep(0.2)
            if ser.in_waiting:
                print("Pico dit :", ser.read(ser.in_waiting).decode(errors='ignore'))
            
            ser.close()
            messagebox.showinfo("Succès", "Touches et Couleurs envoyées !")
            
        except Exception as e:
            messagebox.showerror("Erreur", f"Erreur de communication : {e}")

if __name__ == "__main__":
    root = tk.Tk()
    app = KeyboardConfigurator(root)
    root.mainloop()