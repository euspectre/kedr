<?xml version="1.0" encoding="utf-8"?> 
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<!-- XSL stylesheets to generate KEDR Manual in the format appropriate
	for Google Code Wiki from DocBook XML files. -->

<!-- As it is suggested in chunk.xsl from HTML stylesheet set, we should
	first import XSL with our customization layer, then 
	chunk-common.xsl and after that - include chunk-code.xsl -->   
	<xsl:import href="gc_wiki_impl.xsl"/>
	
	<xsl:import href="@DOCBOOK_XSL_PATH@/html/chunk-common.xsl"/>
	<xsl:include href="@DOCBOOK_XSL_PATH@/html/chunk-code.xsl"/> 
</xsl:stylesheet>
