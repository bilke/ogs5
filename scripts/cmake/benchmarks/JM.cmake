include(test/Benchmark)

Benchmark(AUTHOR JM
	PATH THM/deco_2015_b2s1/deco_b2s1
	CONFIG FEM
	RUNTIME 299
	OUTPUT_FILES deco_b2s1_ply_unten_t0.tec
)

# First component is benchmark name, second is runtime
set(BMB3_LIST
	"m1_1Dlozenge\;11"
	"m1_2Dload\;3"
	"m1_3Dload\;3"
	"m1_3Dtorsion\;21"
	"m1_3Dwaves\;167"
	"m1_3Dwings\;226"
	"m2_1Dlozenge\;1091"
	"m2_1Dlozengebt\;2000"
	"m2_2Dload\;402"
	"m2_2Dloadbt\;1322"
	"m2_3Dload\;445"
	"m2_3Dloadbt\;1622"
	"hm2_1Dcolumn1\;1567"
	"hm2_1Dcolumn2\;221"
	"hm2_2Dmandel\;1511"
	"tm1_2Dbeam\;83"
	"tm1_3Dsquare\;395"
	"tm1_3Dorigin\;52"
)

foreach(BM ${BMB3_LIST})
	list(GET BM 0 BM_NAME)
	list(GET BM 1 BM_RUNTIME)
	Benchmark(AUTHOR JM
		PATH Books/BMB3/Vogel/${BM_NAME}/ogs5/${BM_NAME}
		CONFIG FEM
		OUTPUT_FILES ${BM_NAME}_domain_hex.tec
		RUNTIME ${BM_RUNTIME}
	)
endforeach()
