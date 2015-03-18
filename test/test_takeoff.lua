-- Test des commandes de décollage et d'atterissage (avec contrôle des navdatas)
l=require("libjakopter")
l.connect()

alt = 0
-- Décollage
l.takeoff()
alt = l.cc_read_int(1, 4)
print("Altitude :", alt)

-- Descente
l.move(0,0,-0.2,0)
while alt > 650 do
	-- enregistre la hauteur
	alt = l.cc_read_int(1, 4)
end

l.land()
while alt > 10 do
	-- enregistre la hauteur
	alt = l.cc_read_int(1, 4)
end

print("Altitude :", alt)
l.takeoff()

-- Montée
l.move(0,0,0.5,0)
while alt < 1700 do
	-- enregistre la hauteur
	alt = l.cc_read_int(1, 4)
end

print("Altitude :", alt)

-- Atterissage
l.land()
print("Atterissage")
while alt > 10 do
	-- enregistre la hauteur
	alt = l.cc_read_int(1, 4)
end

l.disconnect()