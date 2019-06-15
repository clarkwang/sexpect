
adoc = sexpect.adoc
man = sexpect.1
html = sexpect.html

to_clean = ${man} ${html}

all: ${man} ${html}

${man}: ${adoc}
	asciidoctor -o - -b manpage $^ | sed 's,\\(rs\\ ,\\(rs\\(rs ,' > $@

${html}: ${adoc}
	asciidoctor -o - -b html $^ > $@

clean:
	rm ${to_clean} 
