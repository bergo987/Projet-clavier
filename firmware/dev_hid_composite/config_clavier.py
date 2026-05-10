import tkinter as tk
from tkinter import messagebox, ttk, colorchooser
import serial
import time
import colorsys
 
# =============================================================================
# CONFIGURATION
# =============================================================================
 
# Port série par défaut — modifiable directement dans l'interface
PORT = 'COM6'
 
# Dictionnaire des touches HID disponibles
# Clé = nom affiché dans l'interface, Valeur = keycode HID USB
HID_KEYS = {
    # Lettres
    "A": 0x04, "B": 0x05, "C": 0x06, "D": 0x07, "E": 0x08, "F": 0x09,
    "G": 0x0A, "H": 0x0B, "I": 0x0C, "J": 0x0D, "K": 0x0E, "L": 0x0F,
    "M": 0x10, "N": 0x11, "O": 0x12, "P": 0x13, "Q": 0x14, "R": 0x15,
    "S": 0x16, "T": 0x17, "U": 0x18, "V": 0x19, "W": 0x1A, "X": 0x1B,
    "Y": 0x1C, "Z": 0x1D,
    # Touches spéciales
    "ENTER":      0x28,
    "ESCAPE":     0x29,
    "BACKSPACE":  0x2A,
    "TAB":        0x2B,
    "SPACE":      0x2C,
    "CAPS_LOCK":  0x39,
    # Modificateurs
    "CTRL_L":     0xE0,  # Contrôle gauche
    "CTRL_R":     0xE4,  # Contrôle droit
    "SHIFT_L":    0xE1,  # Majuscule gauche
    "SHIFT_R":    0xE5,  # Majuscule droite
    "ALT_L":      0xE2,  # Alt gauche
    "ALT_R":      0xE6,  # Alt droit (Alt Gr)
}
 
# =============================================================================
# MAPPING PAR DÉFAUT
# Correspond exactement à la disposition physique du clavier :
#
#  ┌─────────┬─────────┬─────────┬─────────┐
#  │  ESC    │  A      │  B      │  C      │
#  ├─────────┼─────────┼─────────┼─────────┤
#  │  TAB    │  D      │  E      │  F      │
#  ├─────────┼─────────┼─────────┼─────────┤
#  │  CAPS   │  G      │  H      │  I      │
#  ├─────────┼─────────┼─────────┼─────────┤
#  │  SHIFT  │  J      │  K      │  L      │
#  ├─────────┼─────────┼─────────┼─────────┤
#  │  CTRL   │  ALT    │  SPACE  │  ENTER  │
#  └─────────┴─────────┴─────────┴─────────┘
# =============================================================================
DEFAULT_MAPPING = [
    ["ESCAPE",   "A",       "B",     "C"    ],
    ["TAB",      "D",       "E",     "F"    ],
    ["CAPS_LOCK","G",       "H",     "I"    ],
    ["SHIFT_L",  "J",       "K",     "L"    ],
    ["CTRL_L",   "ALT_L",   "SPACE", "ENTER"],
]
 
 
# =============================================================================
# CLASSE PRINCIPALE DE L'INTERFACE
# =============================================================================
class KeyboardConfigurator:
    def __init__(self, root):
        self.root = root
        self.root.title("Pico Keyboard Configurator")
        self.root.geometry("720x820")
        self.root.resizable(False, False)
 
        # ------------------------------------------------------------------
        # SECTION : Connexion série
        # ------------------------------------------------------------------
        port_frame = tk.LabelFrame(root, text=" Connexion ", padx=10, pady=8)
        port_frame.pack(fill="x", padx=15, pady=(12, 4))
 
        tk.Label(port_frame, text="Port COM :").pack(side=tk.LEFT)
        self.port_entry = tk.Entry(port_frame, width=10)
        self.port_entry.insert(0, PORT)
        self.port_entry.pack(side=tk.LEFT, padx=5)
 
        # Indicateur de statut de connexion
        self.status_label = tk.Label(port_frame, text="● Non connecté", fg="red")
        self.status_label.pack(side=tk.LEFT, padx=15)
 
        # ------------------------------------------------------------------
        # SECTION : Mapping des touches (matrice 5x4)
        # ------------------------------------------------------------------
        tk.Label(root, text="MAPPING DES TOUCHES", font=('Arial', 10, 'bold')).pack(pady=(10, 2))
 
        self.key_frame = tk.Frame(root)
        self.key_frame.pack(pady=6)
 
        # Création des menus déroulants pour chaque touche de la matrice
        self.key_vars = []
        for r in range(5):
            row_vars = []
            for c in range(4):
                var = tk.StringVar(root)
                # On initialise avec la valeur du mapping par défaut
                var.set(DEFAULT_MAPPING[r][c])
                cb = ttk.Combobox(
                    self.key_frame,
                    textvariable=var,
                    values=list(HID_KEYS.keys()),
                    width=9,
                    state="readonly"
                )
                cb.grid(row=r, column=c, padx=4, pady=4)
                row_vars.append(var)
            self.key_vars.append(row_vars)
 
        # Bouton pour remettre le mapping par défaut
        tk.Button(
            root, text="↺ Remettre le mapping par défaut",
            command=self.reset_mapping, fg="#555"
        ).pack(pady=(0, 8))
 
        # ------------------------------------------------------------------
        # SECTION : Couleurs des LEDs (mode CUSTOM uniquement)
        # Chaque bouton ouvre un sélecteur de couleur et affiche la teinte choisie
        # ------------------------------------------------------------------
        tk.Label(
            root,
            text="COULEURS DES LEDS — Mode CUSTOM (clic = choisir couleur)",
            font=('Arial', 10, 'bold')
        ).pack(pady=(6, 2))
 
        self.led_frame = tk.Frame(root)
        self.led_frame.pack(pady=6)
 
        # led_hues[i] = teinte HSV (0–255) de la LED i
        self.led_hues = [0] * 20
        self.led_buttons = []
 
        for i in range(20):
            btn = tk.Button(
                self.led_frame,
                text=f"L{i+1:02d}",
                width=6,
                height=2,
                bg="black",
                fg="white",
                relief="raised",
                command=lambda idx=i: self.pick_color(idx)
            )
            btn.grid(row=i // 4, column=i % 4, padx=4, pady=4)
            self.led_buttons.append(btn)
 
        # Boutons pour tout mettre à la même couleur ou tout éteindre
        led_ctrl_frame = tk.Frame(root)
        led_ctrl_frame.pack(pady=(0, 8))
        tk.Button(
            led_ctrl_frame, text="🎨 Tout mettre à la même couleur",
            command=self.fill_all_leds
        ).pack(side=tk.LEFT, padx=6)
        tk.Button(
            led_ctrl_frame, text="⬛ Tout éteindre",
            command=self.clear_all_leds
        ).pack(side=tk.LEFT, padx=6)
 
        # ------------------------------------------------------------------
        # SECTION : Bouton d'envoi principal
        # Envoie 40 octets au Pico : 20 touches + 20 teintes LED
        # ------------------------------------------------------------------
        self.send_btn = tk.Button(
            root,
            text="⚡  FLASHER LA CONFIG  (40 octets → Pico)",
            command=self.send_all,
            bg="#2196F3",
            fg="white",
            font=('Arial', 12, 'bold'),
            height=2,
            width=36,
            relief="raised"
        )
        self.send_btn.pack(pady=12)
 
        # Zone de log pour afficher les messages de debug/retour du Pico
        log_frame = tk.LabelFrame(root, text=" Log ", padx=8, pady=6)
        log_frame.pack(fill="x", padx=15, pady=(0, 12))
        self.log_text = tk.Text(log_frame, height=5, state="disabled", bg="#1e1e1e", fg="#cccccc")
        self.log_text.pack(fill="x")
 
    # --------------------------------------------------------------------------
    # Ouvre le sélecteur de couleur pour une LED donnée
    # --------------------------------------------------------------------------
    def pick_color(self, idx):
        color = colorchooser.askcolor(title=f"Couleur pour LED {idx + 1}")
        if color[1]:  # L'utilisateur n'a pas annulé
            rgb = color[0]  # Tuple (R, G, B) dans [0, 255]
 
            # Conversion RGB → HSV pour extraire uniquement la teinte (hue)
            # Le firmware stocke et utilise la teinte en 0–255 (format SSD1306/HSV)
            h, s, v = colorsys.rgb_to_hsv(rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0)
            self.led_hues[idx] = int(h * 255)
 
            # On met à jour la couleur de fond du bouton pour le retour visuel
            self.led_buttons[idx].config(bg=color[1])
 
    # --------------------------------------------------------------------------
    # Applique la même couleur à toutes les LEDs
    # --------------------------------------------------------------------------
    def fill_all_leds(self):
        color = colorchooser.askcolor(title="Couleur pour toutes les LEDs")
        if color[1]:
            rgb = color[0]
            h, s, v = colorsys.rgb_to_hsv(rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0)
            hue = int(h * 255)
            for i in range(20):
                self.led_hues[i] = hue
                self.led_buttons[i].config(bg=color[1])
 
    # --------------------------------------------------------------------------
    # Éteint toutes les LEDs (teinte = 0, boutons noirs)
    # --------------------------------------------------------------------------
    def clear_all_leds(self):
        for i in range(20):
            self.led_hues[i] = 0
            self.led_buttons[i].config(bg="black")
 
    # --------------------------------------------------------------------------
    # Remet le mapping par défaut dans tous les menus déroulants
    # --------------------------------------------------------------------------
    def reset_mapping(self):
        for r in range(5):
            for c in range(4):
                self.key_vars[r][c].set(DEFAULT_MAPPING[r][c])
 
    # --------------------------------------------------------------------------
    # Ajoute une ligne dans la zone de log
    # --------------------------------------------------------------------------
    def log(self, msg):
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.see(tk.END)  # Scroll automatique vers le bas
        self.log_text.config(state="disabled")
 
    # --------------------------------------------------------------------------
    # Construit et envoie le payload de 40 octets au Pico via le port série
    #
    # Structure du payload :
    #   Octets  0–19 : keycodes HID de la matrice 5×4 (ligne par ligne)
    #   Octets 20–39 : teintes HSV (0–255) des 20 LEDs
    # --------------------------------------------------------------------------
    def send_all(self):
        port = self.port_entry.get().strip()
        payload = []
 
        try:
            # --- 1. Construction des 20 octets de touches ---
            for row in self.key_vars:
                for var in row:
                    key_name = var.get()
                    if key_name not in HID_KEYS:
                        raise ValueError(f"Touche inconnue : '{key_name}'")
                    payload.append(HID_KEYS[key_name])
 
            # --- 2. Ajout des 20 octets de teintes LED ---
            payload.extend(self.led_hues)
 
            # Vérification de la taille avant envoi
            if len(payload) != 40:
                raise ValueError(f"Payload incorrect : {len(payload)} octets au lieu de 40")
 
            self.log(f"→ Connexion sur {port}...")
            self.status_label.config(text="● Connexion...", fg="orange")
            self.root.update()
 
            # Ouverture du port série
            # DTR=False à l'ouverture pour éviter un reset involontaire du Pico
            ser = serial.Serial(
                port,
                baudrate=115200,
                timeout=2,
                write_timeout=2,
                dsrdtr=False,
                rtscts=False
            )
 
            # On active DTR après ouverture — nécessaire pour que le CDC TinyUSB réponde
            ser.dtr = True
            time.sleep(1.5)  # On laisse le temps au Pico de monter l'interface CDC
 
            # Vidage des buffers pour éviter des données résiduelles
            ser.reset_input_buffer()
            ser.reset_output_buffer()
 
            # --- 3. Envoi du payload ---
            self.log(f"→ Envoi de {len(payload)} octets...")
            self.log(f"   Touches : {[hex(b) for b in payload[:20]]}")
            self.log(f"   LEDs    : {[hex(b) for b in payload[20:]]}")
 
            bytes_written = ser.write(bytes(payload))
            ser.flush()
            self.log(f"✓ {bytes_written} octets envoyés.")
 
            # --- 4. Lecture de la réponse du Pico (debug) ---
            time.sleep(1.0)
            if ser.in_waiting:
                response = ser.read(ser.in_waiting).decode(errors='ignore')
                self.log(f"Pico répond : {response}")
            else:
                self.log("(Pas de réponse du Pico)")
 
            ser.close()
 
            self.status_label.config(text="● Envoi réussi", fg="green")
            messagebox.showinfo("Succès", "Configuration envoyée et sauvegardée dans la flash !")
 
        except serial.SerialTimeoutException:
            self.log("✗ TIMEOUT — Le Pico n'accepte pas les données.")
            self.status_label.config(text="● Erreur timeout", fg="red")
            messagebox.showerror(
                "Timeout",
                "Le Pico ne répond pas.\n"
                "→ Vérifie le bon port COM dans le Gestionnaire de périphériques\n"
                "→ Essaie de débrancher / rebrancher le Pico"
            )
        except serial.SerialException as e:
            self.log(f"✗ Erreur série : {e}")
            self.status_label.config(text="● Erreur série", fg="red")
            messagebox.showerror(
                "Erreur série",
                f"{e}\n\n"
                "→ Le port est peut-être déjà ouvert (Arduino IDE, PuTTY...)\n"
                "→ Vérifie que le Pico est bien branché en USB"
            )
        except Exception as e:
            self.log(f"✗ Erreur : {e}")
            self.status_label.config(text="● Erreur", fg="red")
            messagebox.showerror("Erreur", str(e))
 
 
# =============================================================================
# POINT D'ENTRÉE
# =============================================================================
if __name__ == "__main__":
    root = tk.Tk()
    app = KeyboardConfigurator(root)
    root.mainloop()
 