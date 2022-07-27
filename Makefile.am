SUBDIRS = libjxtl \
          src \
          test

ACLOCAL_AMFLAGS = -I m4
TMP_DEST_DIR = $(shell pwd)/tmp_dest_dir

bindings: perl-bindings python-bindings
install-bindings: install-perl-bindings install-python-bindings

BINDINGS_DIR=${top_srcdir}/bindings
PERL_BINDINGS_DIR=${BINDINGS_DIR}/perl
PERL_BINDINGS_FILES=${PERL_BINDINGS_DIR}/LibJXTL.pm ${PERL_BINDINGS_DIR}/libjxtl_wrap.c
PYTHON_BINDINGS_DIR=${BINDINGS_DIR}/python
PYTHON_SWIG_FILES=${PYTHON_BINDINGS_DIR}/libjxtl.py ${PYTHON_BINDINGS_DIR}/libjxtl_wrap.c

${PERL_BINDINGS_FILES}:
	${SWIG} -I${top_srcdir}/libjxtl -I${BINDINGS_DIR}/include -perl5 ${PERL_BINDINGS_DIR}/libjxtl.i

${PERL_BINDINGS_DIR}/Makefile: ${PERL_BINDINGS_DIR}/Makefile.PL
	cd ${PERL_BINDINGS_DIR} && perl Makefile.PL

perl-bindings: ${PERL_BINDINGS_FILES} ${PERL_BINDINGS_DIR}/Makefile
	${MAKE} -C ${PERL_BINDINGS_DIR}

install-perl-bindings:
	${MAKE} -C ${PERL_BINDINGS_DIR} install

perl-rpm: ${PYTHON_SWIG_FILES}
	rm -rf "${TMP_DEST_DIR}"
	mkdir "${TMP_DEST_DIR}"
	cd ${PERL_BINDINGS_DIR} && perl Makefile.PL PREFIX="${TMP_DEST_DIR}"
	make clean
	${MAKE} -C ${PERL_BINDINGS_DIR}
	${MAKE} -C ${PERL_BINDINGS_DIR} install
	fpm -s dir -t rpm -d "${PACKAGE_NAME}" -n "perl-LibJXTL" -v "${PACKAGE_VERSION}" \
		-C "${TMP_DEST_DIR}"
	rm -rf "${TMP_DEST_DIR}"

${PYTHON_SWIG_FILES}: ${PYTHON_BINDINGS_DIR}/libjxtl.i ${BINDINGS_DIR}/include/template.i
	${SWIG} -I${top_srcdir}/libjxtl -I${BINDINGS_DIR}/include -python ${PYTHON_BINDINGS_DIR}/libjxtl.i

python-bindings: ${PYTHON_SWIG_FILES}
	cd ${PYTHON_BINDINGS_DIR} && python3 setup.py build_ext --inplace

install-python-bindings:
	cd ${PYTHON_BINDINGS_DIR} && python3 setup.py install

python-rpm: python-bindings
	rm -rf "${TMP_DEST_DIR}"
	mkdir "${TMP_DEST_DIR}"
	cd ${PYTHON_BINDINGS_DIR} && python3 setup.py build_ext --inplace
	cd ${PYTHON_BINDINGS_DIR} && python3 setup.py install --prefix /usr --root ${TMP_DEST_DIR}
	fpm -f -s dir -t rpm -d "${PACKAGE_NAME}" -n "python3-libjxtl" -C tmp_dest_dir
	rm -rf "${TMP_DEST_DIR}"

rpm:
	rm -rf "${TMP_DEST_DIR}"
	mkdir "${TMP_DEST_DIR}"
	make clean
	${MAKE}
	${MAKE} install DESTDIR="${TMP_DEST_DIR}"
	fpm -f -s dir -t rpm -n "${PACKAGE_NAME}" -v "${PACKAGE_VERSION}" \
		-d "apr-util" \
		-d "apr" \
		-d "expat" \
		-C tmp_dest_dir \
		${libdir}/=${libdir} \
		${bindir}/=${bindir}
	ln -s ${TMP_DEST_DIR}${libdir}/libjxtl-1.0.0.so ${TMP_DEST_DIR}${libdir}/libjxtl.so
	fpm -f -s dir -t rpm -n "${PACKAGE_NAME}-devel" -v "${PACKAGE_VERSION}" \
		-d "apr-util-devel" \
		-d "apr-devel" \
		-d "expat-devel" \
		-C tmp_dest_dir \
		${libdir}/libjxtl-1.0.so=${libdir}/libjxtl.so \
		${includedir}/=${includedir}
	rm -rf "${TMP_DEST_DIR}"
