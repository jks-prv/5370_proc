F = 5370
OF = 5370-v3.1

# tar up the files for submission to OSH Park
.PHONY: all
all:
	zip $(OF).zip $(F).drl $(F)-Edge_Cuts.gbr $(F)-F_SilkS.gto $(F)-Front.gtl $(F)-F_Mask.gts $(F)-B_SilkS.gbo $(F)-Back.gbl $(F)-B_Mask.gbs
#	zip $(OF).zip $(F).drl $(F)-NPTH.drl $(F)-Edge_Cuts.gbr $(F)-F_SilkS.gto $(F)-Front.gtl $(F)-F_Mask.gts $(F)-B_SilkS.gbo $(F)-Back.gbl $(F)-B_Mask.gbs

# convert the .ps created by the newpcb print command into a .pdf
.PHONY: pdf
pdf:
	ps2pdf $(F).ps $(F).pdf
