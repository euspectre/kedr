<?xml version="1.0" encoding="utf-8"?> 
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:import href="@DOCBOOK_XSL_PATH@/html/chunk.xsl"/>
    <xsl:param name="chunker.output.encoding" select="'utf-8'"/>
    <xsl:param name="chunk.first.sections" select="1"/>
    <xsl:param name="html.stylesheet" select="'kedr-doc.css'"/>
    <xsl:param name="use.id.as.filename" select="1"/>
    <xsl:param name="section.autolabel" select="1"/>
    <xsl:param name="generate.toc">
book      toc,title
article   toc,title
chapter   toc
section   toc
appendix  toc
    </xsl:param>
    <xsl:param name="generate.section.toc.level" select="1"/>
    <xsl:param name="toc.section.depth" select="3"/>

    <xsl:template match="*" mode="html.title.attribute"/>
<!-- 
The above rule disables generation of "title" attributes, and hence, those
annoying tooltips will no longer be shown when the mouse pointer is hovering
over the text.

For details, see 
@DOCBOOK_XSL_PATH@/html/sections.xsl
-->
</xsl:stylesheet>
