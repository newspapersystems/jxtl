AM_CPPFLAGS = -I${top_srcdir}/libjxtl
bin_PROGRAMS = jxtl xml2json jxtl-json-verify
if STATIC_XML2JSON
bin_PROGRAMS += xml2json_static
endif
jxtl_SOURCES = jxtl.c
xml2json_SOURCES = xml2json.c
xml2json_static_SOURCES = $(xml2json_SOURCES)
jxtl_json_verify_SOURCES = json_verify.c

LIBJXTL = ${top_srcdir}/libjxtl/libjxtl-1.0.la

jxtl_CFLAGS = -g ${APR_CFLAGS} ${APU_CFLAGS}
jxtl_LDFLAGS = ${APR_LIBS} ${APU_LIBS}
jxtl_LDADD = ${LIBJXTL}

xml2json_CFLAGS = -g ${APR_CFLAGS} ${APU_CFLAGS}
xml2json_LDFLAGS = ${APR_LIBS} ${APU_LIBS}
xml2json_LDADD = ${LIBJXTL}

xml2json_static_CFLAGS = -static $(xml2json_CFLAGS)
xml2json_static_LDFLAGS = -static $(xml2json_LDFLAGS)
xml2json_static_LDADD = ${LIBJXTL}

jxtl_json_verify_CFLAGS = -g ${APR_CFLAGS} ${APU_CFLAGS}
jxtl_json_verify_LDFLAGS = ${APR_LIBS} ${APU_LIBS}
jxtl_json_verify_LDADD = ${LIBJXTL}
