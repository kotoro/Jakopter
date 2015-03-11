--Boucle de contrôle du drone + affichage vidéo

l=require("libjakopter")
l.connect()
--Canal de com navdata (lecture des données)
ccn = l.get_cc(1)
--Envoi des navdata au module vidéo
--vérifier le timestamp pour voir si de nouvelles données sont arrivées
--tt = l.get_cc_time(ccv)
alt = 0
-- Décollage
l.takeoff()
--while alt < 1000 do
	-- enregistre la hauteur
	alt = l.read_int(ccn, 4)
	print("Altitude :", alt)
--end

-- Atterissage
l.land()
--while alt > 10 do
	-- enregistre la hauteur
	alt = l.read_int(ccn, 4)
	print("Altitude :", alt)
--end

l.disconnect()