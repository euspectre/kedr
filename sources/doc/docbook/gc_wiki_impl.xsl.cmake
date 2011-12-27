<?xml version="1.0" encoding="utf-8"?> 
<xsl:stylesheet version="1.0"
		xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<!-- The XSL stylesheets used in this package to prepare KEDR manual 
	in the format appropriate for Wiki are based on the official 
	DocBook XSL stylesheets for HTML. See docbook_xsl_copying.txt for
	detailed copyright and license information. -->

<!-- Our stylesheets were tested and confirmed to work correctly with 
	DocBook 4.5 XSL stylesheets version 1.75.2. We can not guarantee that
	everything will work with other versions of DocBook XSL stylesheets
	but it might be.  -->

<!-- We use some of the XSL stylesheets for chunked HTML to generate Wiki 
	pages, the process is similar in both cases to some extent. -->
<xsl:import href="@DOCBOOK_XSL_PATH@/html/docbook.xsl"/>

<xsl:output method="text"/>
<!-- ================================================================ -->

<xsl:param name="file.ext" select="'.wiki'"/>
<xsl:param name="html.ext" select="$file.ext"/>
<xsl:param name="root.filename" select="'kedr_manual'"/>

<xsl:param name="chunker.output.encoding" select="'utf-8'"/>
<xsl:param name="chunker.output.method" select="'text'"/>
<xsl:param name="chunk.first.sections" select="1"/>

<xsl:param name="use.id.as.filename" select="1"/>
<xsl:param name="section.autolabel" select="0"/>
<xsl:param name="suppress.navigation" select="1"/>

<!-- Maximum number of levels in the auto-generated tables of contents. -->
<xsl:param name="wiki.max_toc_depth" select="4"/>

<!-- Create a heading with the given level for each entry in the glossary.
	This is needed to establish links to these entries. -->
<xsl:param name="wiki.glossentry_hlevel" select="3"/>

<!-- Maximum length of the "anchor" part in the links (the one after '#').
	If that part is longer, the link should be generated without it. -->
<xsl:param name="wiki.max.anchor.length" select="64"/>

<!-- The path to KEDR documentation in DocBook format in the repository.
	This is used to provide correct links to the images. -->
<xsl:param name="repo_doc_path" 
	select="'http://kedr.googlecode.com/hg/sources/doc/docbook/'"/>
<!-- ================================================================ -->

<xsl:variable name="newline">
<xsl:text>
</xsl:text>
</xsl:variable>
<!-- ================================================================ -->

<!-- Make sure only the standard quotation marks are used for <quote> -->
<!-- Overridden from gentext.xsl -->
<xsl:template name="gentext.startquote">
	<xsl:text>&quot;</xsl:text>
</xsl:template>

<xsl:template name="gentext.endquote">
	<xsl:text>&quot;</xsl:text>
</xsl:template>
<!-- ================================================================ -->

<xsl:template match="emphasis">
	<xsl:text>*</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>*</xsl:text>
</xsl:template>

<xsl:template match="programlisting/emphasis|screen/emphasis
											|programlisting/filename|screen/filename
											|programlisting/replaceable|screen/replaceable
											|programlisting/command|screen/command">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="phrase[@role='emphasized']">
	<xsl:text>&lt;b&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/b&gt;</xsl:text>
</xsl:template>

<xsl:template match="phrase[@role='pcite']">
	<xsl:text>&lt;i&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/i&gt;</xsl:text>
</xsl:template>

<xsl:template match="code|type|function|varname|constant|filename
										|literal">
	<xsl:text>{{{</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>}}}</xsl:text>
</xsl:template>

<xsl:template match="code/type|code/function|code/varname
											|code/constant|code/literal">
	<xsl:apply-templates/>
</xsl:template>

<xsl:template match="command">
	<xsl:text>&lt;b&gt;{{{</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>}}}&lt;/b&gt;</xsl:text>
</xsl:template>

<xsl:template match="para">
	<xsl:value-of select="$newline"/>
	<xsl:apply-templates/>
	<xsl:value-of select="$newline"/>
</xsl:template>

<xsl:template match="programlisting|screen">
	<xsl:value-of select="$newline"/>
	<xsl:text>{{{</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>}}}</xsl:text>
	<xsl:value-of select="$newline"/>
</xsl:template>

<xsl:template match="replaceable">
	<xsl:text>_</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>_</xsl:text>
</xsl:template>

<xsl:template match="firstterm">
	<xsl:text>&lt;i&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/i&gt;</xsl:text>
</xsl:template>

<xsl:template match="link/firstterm|ulink/firstterm
											|link/command|ulink/command
											|link/emphasis|ulink/emphasis">
	<xsl:apply-templates/>
</xsl:template>

<!-- Suppress quotation marks in the titles, they may be a problem when 
	creating links. -->
<xsl:template match="title//quote">
	<xsl:apply-templates/>
</xsl:template>
<!-- ================================================================ -->

<!-- If the first character in $str is a lowercase latin letter,
	capitalize that letter and return the resulting string. Otherwise,
	return $str as it is. -->
<xsl:template name="util.cap_first">
	<xsl:param name="str"/>
	
	<xsl:choose>
		<xsl:when test="string-length($str) != 0">
				<xsl:variable name="first" 
					select="translate(substring($str, 1, 1), 
						'abcdefghijklmnopqrstuvwxyz', 
						'ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
				<xsl:variable name="rest" select="substring($str, 2)"/>
				<xsl:value-of select="concat($first, $rest)"/>
		</xsl:when>
	
		<xsl:otherwise>
			<xsl:text></xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- Output the name of the block depending on the given type.
	The name is created from the name of the type by making the first
	character uppercase (if it was a lovercase latin letter).
	<font> tag is used to the colour of the font.
	[NB] Use 'colour' parameter to pass the colour value. If it is an 
	empty string, the default colour will be used. -->
<xsl:template name="make_block_name">
	<xsl:param name="block_type" select="'unknown block type'"/>
	<xsl:param name="colour" select="''"/>

	<xsl:text>&lt;font</xsl:text>
	<xsl:if test="$colour != ''">
		<xsl:text> color=&quot;</xsl:text>
		<xsl:value-of select="$colour"/>
		<xsl:text>&quot;</xsl:text>
	</xsl:if>
	<xsl:text>&gt;&lt;b&gt;&lt;u&gt;</xsl:text>
	
	<xsl:call-template name="util.cap_first">
		<xsl:with-param name="str" select="$block_type"/>
	</xsl:call-template>
	
	<xsl:text>&lt;/u&gt;&lt;/b&gt;&lt;/font&gt;</xsl:text>
</xsl:template>

<xsl:template match="note|warning|important|tip|caution">
	<xsl:value-of select="$newline"/>
	<xsl:text>&lt;blockquote&gt;</xsl:text>
	
	<xsl:variable name="block_type" select="name(.)"/>
	<xsl:variable name="colour">
		<xsl:choose>
			<xsl:when test="$block_type = 'warning'">
				<xsl:text>#FF0000</xsl:text>
			</xsl:when>
			
			<xsl:otherwise>
				<xsl:text></xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	
	<xsl:call-template name="make_block_name">
		<xsl:with-param name="block_type" select="$block_type"/>
		<xsl:with-param name="colour" select="$colour"/>
	</xsl:call-template>
	
	<xsl:apply-templates/>
	
	<xsl:text>&lt;/blockquote&gt;</xsl:text>
	<xsl:value-of select="$newline"/>
</xsl:template>
<!-- ================================================================ -->

<!-- Repeat string 'str' 'ntotal' times -->
<xsl:template name="util.repeat_string">
	<xsl:param name="str" select="'?'"/>
	<xsl:param name="ntotal" select="1"/>

	<xsl:if test="$ntotal &gt; 0">	
		<xsl:variable name="rest_str">
			<xsl:call-template name="util.repeat_string">
				<xsl:with-param name="str">
					<xsl:value-of select="$str"/>
				</xsl:with-param>
				<xsl:with-param name="ntotal">
					<xsl:value-of select="$ntotal - 1"/>
				</xsl:with-param>
			</xsl:call-template>
		</xsl:variable>
		<xsl:value-of select="concat($str, $rest_str)"/>
	</xsl:if>
</xsl:template>
<!-- ================================================================ -->

<!-- Section headings -->
<xsl:template name="section.heading">
	<xsl:param name="section" select="."/>
	<xsl:param name="level" select="1"/>
	<xsl:param name="allow-anchors" select="1"/>
	<xsl:param name="title"/>
	<xsl:param name="class" select="'title'"/>

	<!-- HTML H level is one higher than section level -->
	<xsl:variable name="hlevel">
		<xsl:choose>
			<!-- highest valid HTML H level is H6; so anything nested deeper
					 than 5 levels down just becomes H6 -->
			<xsl:when test="$level &gt; 5">6</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$level + 1"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	
	<!-- An appropriate number of "=" chars -->
	<xsl:variable name="heading.decoration">
		<xsl:call-template name="util.repeat_string">
			<xsl:with-param name="str" select="'='"/>
			<xsl:with-param name="ntotal">
				<xsl:value-of select="$hlevel - 1"/>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:variable>
	
	<xsl:value-of select="$newline"/>
	<xsl:value-of select="concat($heading.decoration, ' ')"/>
	<xsl:copy-of select="$title"/>
	<xsl:value-of select="concat(' ', $heading.decoration)"/>
	<xsl:value-of select="$newline"/>

</xsl:template>

<xsl:template match="glossary">
	<xsl:variable name="heading.decoration" select="'='"/>
	<xsl:variable name="gtitle">
		<xsl:choose>
			<xsl:when test="title">
				<xsl:value-of select="title"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>Glossary</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	
	<xsl:value-of select="$newline"/>
	<xsl:value-of select="concat($heading.decoration, ' ')"/>
	<xsl:value-of select="$gtitle"/>
	<xsl:value-of select="concat(' ', $heading.decoration)"/>
	<xsl:value-of select="$newline"/>
	
	<xsl:apply-templates/>
</xsl:template>
<!-- ================================================================ -->

<!-- Find the name of the file containing the target. It is the id of 
	the topmost section/appendix/glossary ancestor of the target. 
	Assumes $use.id.as.filename != 0. 
	
	[NB] Simplified version of the templates from chunk-code.xsl.
-->
<xsl:template match="*" mode="lookup-filename">
	<xsl:variable name="ischunk">
		<xsl:call-template name="chunk"/>
	</xsl:variable> 
	
	<xsl:variable name="filename">
		<xsl:choose>
			<!-- if this is the root element, use the root.filename -->
			<xsl:when test="not(parent::*) and $root.filename != ''">
				<xsl:value-of select="$root.filename"/>
			</xsl:when>

			<!-- if there's no dbhtml filename, and if we're to use IDs as -->
			<!-- filenames, then use the ID to generate the filename. -->
			<xsl:when test="(@id or @xml:id) and $use.id.as.filename != 0">
				<xsl:value-of select="(@id|@xml:id)[1]"/>
			</xsl:when>
			<xsl:otherwise></xsl:otherwise>
		</xsl:choose>
	</xsl:variable>

	<xsl:choose>
		<xsl:when test="$ischunk='0'">
			<!-- if called on something that isn't a chunk, walk up... -->
			<xsl:choose>
				<xsl:when test="count(parent::*) &gt; 0">
					<xsl:apply-templates mode="lookup-filename" select="parent::*"/>
				</xsl:when>
				<!-- unless there is no up, in which case return "" -->
				<xsl:otherwise></xsl:otherwise>
			</xsl:choose>
		</xsl:when>

		<xsl:when test="$filename != ''">
			<!-- if this chunk has an explicit name, use it -->
			<xsl:value-of select="$filename"/>
		</xsl:when>

		<xsl:otherwise>
			<xsl:text>chunk-filename-error-</xsl:text>
			<xsl:value-of select="name(.)"/>
			<xsl:number level="any" format="01" from="set"/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>
<!-- ================================================================ -->

<!-- Create a Wiki-style link to the text fragment corresponding to the
	target node. '$text' is used as the display text of the link -->
<xsl:template name="make_wiki_link">
	<xsl:param name="target"/>
	<xsl:param name="text" select="'WARNING: no link text set'"/>
	
	<xsl:if test="not($target)">
			<xsl:message terminate="yes">
				<xsl:text>ERROR: link to a non-existent target in </xsl:text>
				<xsl:value-of select="name(.)"/> 
				<xsl:text> to the element with ID &quot;</xsl:text>
				<xsl:value-of select="@linkend"/> 
				<xsl:text>&quot;</xsl:text>
			</xsl:message>
	</xsl:if>
	
	<!-- Find the file the target belongs to, it will be the first part 
		of the link. -->
	<xsl:variable name="filename">
		<xsl:apply-templates mode="lookup-filename" select="$target"/>
	</xsl:variable>
	
	<xsl:text>[</xsl:text>
	<xsl:value-of select="$filename"/>
	
	<!-- Check if the target element has a title or is an entry in the 
		glossary. If so, add its title as an anchor specification to the link, 
		with spaces replaced with underscores. -->
	
	<!-- Make an anchor if it is not too long -->
	<xsl:variable name="anchor_part">
		<xsl:choose>
			<xsl:when test="$target/title">
				<xsl:text>#</xsl:text>
				<xsl:value-of select="translate(normalize-space($target/title), ' ', '_')"/>
			</xsl:when>
			
			<!-- A glossary entry -->
			<xsl:when test="$target/glossterm">
				<xsl:text>#</xsl:text>
				<xsl:value-of select="translate(normalize-space($target/glossterm), ' ', '_')"/>
			</xsl:when>
				
			<xsl:otherwise>
				<xsl:text></xsl:text>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	
	<xsl:if test="string-length($anchor_part) &lt;= $wiki.max.anchor.length + 1">
		<xsl:value-of select="$anchor_part"/>
	</xsl:if>
	
	<!-- Set the text -->
	<xsl:text> </xsl:text>
	<xsl:value-of select="$text"/>
	<xsl:text>]</xsl:text>
</xsl:template>

<!-- xref (overridden from xref.xsl) -->
<xsl:template match="xref" name="xref">
	<xsl:param name="xhref" select="''"/>
	<xsl:param name="xlink.idref"  select="''"/>
	<xsl:param name="xlink.targets" select="key('id',$xlink.idref)"/>
	<xsl:param name="linkend.targets" select="key('id',@linkend)"/>
	<xsl:param name="target" select="($xlink.targets | $linkend.targets)[1]"/>

	<xsl:variable name="text">
		<xsl:text>&quot;</xsl:text>
		<xsl:choose>
			<xsl:when test="$target/title">
				<xsl:value-of select="$target/title"/>
			</xsl:when>
			<xsl:when test="$target/glossterm">
				<xsl:value-of select="$target/glossterm"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text></xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>&quot;</xsl:text>
	</xsl:variable>
	
	<xsl:call-template name="make_wiki_link">
		<xsl:with-param name="target" select="$target"/>
		<xsl:with-param name="text" select="$text"/>
	</xsl:call-template>
</xsl:template>

<!-- link (overridden from xref.xsl) -->
<xsl:template match="link" name="link">
  <xsl:param name="linkend" select="@linkend"/>
  <xsl:param name="a.target"/>
  <xsl:param name="xhref" select="''"/>
  
  <xsl:variable name="linkend.targets" select="key('id',@linkend)"/>
	<xsl:variable name="target" select="($linkend.targets)[1]"/>
	<xsl:variable name="text">
		<xsl:apply-templates/>
	</xsl:variable>
	
	<xsl:call-template name="make_wiki_link">
		<xsl:with-param name="target" select="$target"/>
		<xsl:with-param name="text" select="$text"/>
	</xsl:call-template>
</xsl:template>

<!-- ulink (overridden from xref.xsl) -->
<xsl:template match="ulink" name="ulink">
	<xsl:param name="url" select="@url"/>
	
	<xsl:text>[</xsl:text>
	<xsl:value-of select="$url"/>
	<xsl:text> </xsl:text>
	<xsl:apply-templates/>
	<xsl:text>]</xsl:text>
</xsl:template> 
<!-- ================================================================ -->

<xsl:template name="make_target_title">
	<xsl:param name="target_id" select="''"/>
	
	<xsl:choose>
		<xsl:when test="$target_id != ''">
			<xsl:variable name="target" select="key('id', $target_id)[1]"/>
			<xsl:variable name="anchor">
				<xsl:choose>
					<xsl:when test="$target/title">
						<xsl:value-of select="translate(normalize-space($target/title), ' ', '_')"/>
					</xsl:when>
					
					<xsl:when test="$target/glossterm">
						<xsl:value-of select="translate(normalize-space($target/glossterm), ' ', '_')"/>
					</xsl:when>
						
					<xsl:otherwise>
						<xsl:text></xsl:text>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:variable>
			<xsl:if test="string-length($anchor) &lt;= $wiki.max.anchor.length">
				<xsl:value-of select="$anchor"/>
			</xsl:if>
		</xsl:when>
		
		<xsl:otherwise>
			<xsl:text></xsl:text>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<!-- Similar to "make_wiki_link" but uses the HTML-like href expression
	to determine the destination file and the item in it. The expression
	must have one of the following forms: 
	- <filename>.wiki - the target is page <filename>
	- <filename>.wiki#<id> - the target is item with ID <id> at the page 
		<filename>
	-->
<xsl:template name="make_wiki_link_from_href">
	<xsl:param name="href_expr" select="'WARNING_NO_HREF_SET'"/>
	<xsl:param name="text" select="'WARNING: no link text set'"/>
	
	<xsl:variable name="full_name">
		<xsl:choose>
			<xsl:when test="contains($href_expr, '#')">
				<xsl:value-of select="substring-before($href_expr, '#')"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$href_expr"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	
	<xsl:variable name="target_id">
		<xsl:choose>
			<xsl:when test="contains($href_expr, '#')">
				<xsl:value-of select="substring-after($href_expr, '#')"/>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="''"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	
	<xsl:if test="not(contains($full_name, $file.ext))">
			<xsl:message terminate="yes">
				<xsl:text>Spurious href expression: '</xsl:text>
				<xsl:value-of select="$href_expr"/>
				<xsl:text>', file extension is not '</xsl:text>
				<xsl:value-of select="$file.ext"/>
				<xsl:text>'.</xsl:text>
			</xsl:message>
	</xsl:if>
	
	<xsl:variable name="filename" select="substring-before($full_name, $file.ext)"/>
	<xsl:variable name="target_title">
		<xsl:call-template name="make_target_title">
			<xsl:with-param name="target_id" select="$target_id"/>
		</xsl:call-template>
	</xsl:variable>
	
	<xsl:variable name="full_ref">
		<xsl:choose>
			<xsl:when test="$target_title != ''">
				<xsl:value-of select="concat($filename, '#', $target_title)"/>
			</xsl:when>
			
			<xsl:otherwise>
				<xsl:value-of select="$filename"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>

	<xsl:text>[</xsl:text>
	<xsl:value-of select="$full_ref"/>
	<xsl:text> </xsl:text>
	<xsl:value-of select="$text"/>
	<xsl:text>]</xsl:text>
</xsl:template>
<!-- ================================================================ -->

<!-- Override processing of the page title, etc. -->
<xsl:template name="head.content">
	<xsl:param name="node" select="."/>
	<xsl:param name="title">
		<xsl:value-of select="normalize-space($node/title)"/>
	</xsl:param>
	
	<xsl:variable name="filename">
		<xsl:apply-templates mode="lookup-filename" select="."/>
	</xsl:variable>
	
	<!-- Generate special heading and table of contents if needed -->
	<xsl:choose>
		<xsl:when test="$filename = $root.filename">
			<xsl:text>#summary KEDR Manual: Title Page and Table of Contents</xsl:text>
			<xsl:value-of select="$newline"/>
		</xsl:when>
		
		<xsl:otherwise>
			<xsl:if test="$title">
				<xsl:text>#summary KEDR Manual: </xsl:text>
				<xsl:value-of select="$title"/>
				<xsl:value-of select="$newline"/>
				
				<xsl:value-of select="$newline"/>
				<xsl:text>&lt;wiki:toc max_depth=&quot;</xsl:text>
				<xsl:value-of select="$wiki.max_toc_depth"/>
				<xsl:text>&quot; /&gt;</xsl:text>
				<xsl:value-of select="$newline"/>
			</xsl:if>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>  
<!-- ================================================================ -->

<!-- Processing of the entries in the glossary: create headings for each 
	entry -->
<xsl:template match="glossentry/glossterm">
	<xsl:variable name="heading.decoration">
		<xsl:call-template name="util.repeat_string">
			<xsl:with-param name="str" select="'='"/>
			<xsl:with-param name="ntotal">
				<xsl:value-of select="$wiki.glossentry_hlevel"/>
			</xsl:with-param>
		</xsl:call-template>
	</xsl:variable>
	
	<xsl:value-of select="$newline"/>
	<xsl:value-of select="concat($heading.decoration, ' ')"/>
	<xsl:value-of select="."/>
	<xsl:value-of select="concat(' ', $heading.decoration)"/>
	<xsl:value-of select="$newline"/>	
</xsl:template>
<!-- ================================================================ -->

<!-- Process references to the images 
	[NB] If the path to the image starts with "image/", assume it is a 
	path to a file in the repository and create the full path accordingly.
	Other kinds of paths should remain unchanged. -->
<xsl:template match="mediaobject">
	<xsl:choose>
		<xsl:when test="imageobject/imagedata/@fileref">
			<xsl:variable name="fileref">
				<xsl:value-of select="imageobject/imagedata/@fileref"/>
			</xsl:variable>
			
			<xsl:value-of select="$newline"/>
			<xsl:choose>
				<xsl:when test="starts-with($fileref, 'images/')">
					<xsl:value-of select="concat($repo_doc_path, $fileref)"/>
				</xsl:when>
				
				<!-- The path does not start with "images/", leave it as it is -->
				<xsl:otherwise>
					<xsl:value-of select="$fileref"/>
				</xsl:otherwise>
			</xsl:choose>
			<xsl:value-of select="$newline"/>
		</xsl:when>
		
		<xsl:otherwise>
			<!-- Not an image or unknown specification, may be other templates 
				suit better. -->
			<xsl:apply-templates/>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>
<!-- ================================================================ -->

<!-- <cmdsynopsis> and its fields -->
<xsl:template match="cmdsynopsis">
	<xsl:value-of select="$newline"/>
	<xsl:apply-templates/>
	<xsl:value-of select="$newline"/>
</xsl:template> 

<xsl:template match="cmdsynopsis//arg">
	<xsl:value-of select="' '"/>
	<xsl:apply-templates/>
	<xsl:value-of select="' '"/>
</xsl:template> 

<xsl:template match="cmdsynopsis/group">
	<xsl:text>`[`</xsl:text>
	<xsl:for-each select="arg">
		<xsl:apply-templates select="."/>
		<xsl:if test="not(position()=last())">
			<xsl:text>|</xsl:text>
		</xsl:if>
	</xsl:for-each>
	<xsl:text>...`]`</xsl:text>
</xsl:template>
<!-- ================================================================ -->

<!-- inline <simplelist> -->
<xsl:template match="simplelist[@type='inline']">
	<xsl:for-each select="member">
		<xsl:apply-templates select="."/>
		<xsl:if test="not(position()=last())">
			<xsl:text>, </xsl:text>
		</xsl:if>
	</xsl:for-each>
</xsl:template>

<!-- all other kinds of <simplelist> -->
<xsl:template match="simplelist">
	<xsl:value-of select="$newline"/>
	<xsl:text>&lt;ul&gt;</xsl:text>
	<xsl:for-each select="member">
		<xsl:text>&lt;li&gt;</xsl:text>
		<xsl:apply-templates select="."/>
		<xsl:text>&lt;/li&gt;</xsl:text>
		<xsl:value-of select="$newline"/>
	</xsl:for-each>
	<xsl:text>&lt;/ul&gt;</xsl:text>
</xsl:template>
<!-- ================================================================ -->

<!-- <itemizedlist> and <orderedlist> -->
<xsl:template match="itemizedlist">
	<xsl:value-of select="$newline"/>
	<xsl:text>&lt;ul&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/ul&gt;</xsl:text>
</xsl:template>

<xsl:template match="orderedlist|procedure">
	<xsl:value-of select="$newline"/>
	<xsl:text>&lt;ol&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/ol&gt;</xsl:text>
</xsl:template>

<xsl:template match="itemizedlist/listitem|orderedlist/listitem
											|procedure/step">
	<xsl:text>&lt;li&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/li&gt;</xsl:text>
	<xsl:value-of select="$newline"/>
</xsl:template>

<!-- variablelist -->
<xsl:template match="variablelist">
	<xsl:value-of select="$newline"/>
	<xsl:text>&lt;ul&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/ul&gt;</xsl:text>
</xsl:template>

<xsl:template match="varlistentry">
	<xsl:text>&lt;li&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/li&gt;</xsl:text>
	<xsl:value-of select="$newline"/>
</xsl:template>

<xsl:template match="varlistentry/term">
	<xsl:text>&lt;b&gt;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&lt;/b&gt;</xsl:text>
	<xsl:value-of select="$newline"/>
	<xsl:value-of select="$newline"/>
</xsl:template>

<xsl:template match="varlistentry/listitem">
	<xsl:apply-templates/>
</xsl:template>
<!-- ================================================================ -->

<!-- Title page -->

<xsl:template match="articleinfo" mode="titlepage.mode">
	<xsl:value-of select="$newline"/>
	<xsl:apply-imports/>
	<xsl:value-of select="$newline"/>	
</xsl:template>

<xsl:template match="authorgroup" mode="titlepage.mode">
	<xsl:value-of select="$newline"/>
	
	<xsl:text>_</xsl:text>
	<xsl:variable name="nauthors" select="count(author)"/>
	<xsl:choose>
		<xsl:when test="$nauthors = 1">
			<xsl:text>Author: </xsl:text>
		</xsl:when>
		<xsl:otherwise>
			<xsl:text>Authors: </xsl:text>
		</xsl:otherwise>
	</xsl:choose>
	
	<xsl:apply-templates mode="titlepage.mode"/>
	<xsl:text>_</xsl:text>
	<xsl:value-of select="$newline"/>
</xsl:template>

<xsl:template match="author" mode="titlepage.mode">
	<xsl:apply-imports/>
	<xsl:if test="position() != last()">
		<xsl:text>, </xsl:text>
	</xsl:if>
</xsl:template>

<xsl:template match="article/title" mode="titlepage.mode">
	<xsl:value-of select="$newline"/>
	<xsl:text>= </xsl:text>
	<xsl:apply-imports/>
	<xsl:text> =</xsl:text>
	<xsl:value-of select="$newline"/>	
</xsl:template>

<xsl:template match="releaseinfo" mode="titlepage.mode">
	<xsl:value-of select="$newline"/>
	<xsl:text>*</xsl:text>
	<xsl:apply-imports/>
	<xsl:text>*</xsl:text>
	<xsl:value-of select="$newline"/>	
</xsl:template>

<xsl:template match="copyright" mode="titlepage.mode">
	<xsl:value-of select="$newline"/>
	<xsl:apply-imports/>
	<xsl:value-of select="$newline"/>	
</xsl:template>

<xsl:template match="legalnotice" mode="titlepage.mode">
	<xsl:value-of select="$newline"/>
	<xsl:apply-imports/>
	<xsl:value-of select="$newline"/>	
</xsl:template>

<!-- ================================================================ -->

<!-- Main table of contents (based on the templates from autotoc.xsl) -->
<xsl:template name="make.toc">
	<xsl:param name="toc-context" select="."/>
	<xsl:param name="toc.title.p" select="true()"/>
	<xsl:param name="nodes" select="/NOT-AN-ELEMENT"/>

	<xsl:variable name="nodes.plus" select="$nodes"/>

	<xsl:variable name="toc.title">
		<xsl:value-of select="$newline"/>
		<xsl:text>== Table of Contents ==</xsl:text>
		<xsl:value-of select="$newline"/>
	</xsl:variable>

	<xsl:if test="$nodes">
		<xsl:copy-of select="$toc.title"/>
		<xsl:value-of select="$newline"/>	

		<xsl:text>&lt;dl&gt;</xsl:text>
		<xsl:value-of select="$newline"/>	
		<xsl:apply-templates select="$nodes" mode="toc">
			<xsl:with-param name="toc-context" select="$toc-context"/>
		</xsl:apply-templates>
		<xsl:value-of select="$newline"/>	
		<xsl:text>&lt;/dl&gt;</xsl:text>

		<xsl:value-of select="$newline"/>	
	</xsl:if>
</xsl:template>

<!-- Process an item of the TOC -->
<xsl:template name="toc.line">
	<xsl:param name="toc-context" select="."/>
	<xsl:param name="depth" select="1"/>
	<xsl:param name="depth.from.context" select="8"/>
	
	<xsl:variable name="label">
		<xsl:apply-templates select="." mode="label.markup"/>
	</xsl:variable>
	
	<xsl:variable name="text">
		<xsl:copy-of select="$label"/>
		<xsl:if test="$label != ''">
			<xsl:value-of select="$autotoc.label.separator"/>
		</xsl:if>
		<xsl:apply-templates select="." mode="titleabbrev.markup"/>
	</xsl:variable>

	<xsl:variable name="href_expr">
		<xsl:call-template name="href.target">
			<xsl:with-param name="context" select="$toc-context"/>
			<xsl:with-param name="toc-context" select="$toc-context"/>
		</xsl:call-template>
	</xsl:variable>
	
	<xsl:call-template name="make_wiki_link_from_href">
		<xsl:with-param name="href_expr" select="$href_expr"/>
		<xsl:with-param name="text" select="$text"/>
	</xsl:call-template>
</xsl:template>

<!-- Handle multi-level TOC -->
<xsl:template name="subtoc">
	<xsl:param name="toc-context" select="."/>
	<xsl:param name="nodes" select="NOT-AN-ELEMENT"/>

	<xsl:variable name="subtoc">
		<xsl:text>&lt;dl&gt;</xsl:text>
		<xsl:apply-templates mode="toc" select="$nodes">
			<xsl:with-param name="toc-context" select="$toc-context"/>
		</xsl:apply-templates>
		<xsl:text>&lt;/dl&gt;</xsl:text>
		<xsl:value-of select="$newline"/>
	</xsl:variable>

	<xsl:variable name="depth">
		<xsl:choose>
			<xsl:when test="local-name(.) = 'section'">
				<xsl:value-of select="count(ancestor::section) + 1"/>
			</xsl:when>
			<xsl:when test="local-name(.) = 'sect1'">1</xsl:when>
			<xsl:when test="local-name(.) = 'sect2'">2</xsl:when>
			<xsl:when test="local-name(.) = 'sect3'">3</xsl:when>
			<xsl:when test="local-name(.) = 'sect4'">4</xsl:when>
			<xsl:when test="local-name(.) = 'sect5'">5</xsl:when>
			<xsl:when test="local-name(.) = 'refsect1'">1</xsl:when>
			<xsl:when test="local-name(.) = 'refsect2'">2</xsl:when>
			<xsl:when test="local-name(.) = 'refsect3'">3</xsl:when>
			<xsl:when test="local-name(.) = 'simplesect'">
				<!-- sigh... -->
				<xsl:choose>
					<xsl:when test="local-name(..) = 'section'">
						<xsl:value-of select="count(ancestor::section)"/>
					</xsl:when>
					<xsl:when test="local-name(..) = 'sect1'">2</xsl:when>
					<xsl:when test="local-name(..) = 'sect2'">3</xsl:when>
					<xsl:when test="local-name(..) = 'sect3'">4</xsl:when>
					<xsl:when test="local-name(..) = 'sect4'">5</xsl:when>
					<xsl:when test="local-name(..) = 'sect5'">6</xsl:when>
					<xsl:when test="local-name(..) = 'refsect1'">2</xsl:when>
					<xsl:when test="local-name(..) = 'refsect2'">3</xsl:when>
					<xsl:when test="local-name(..) = 'refsect3'">4</xsl:when>
					<xsl:otherwise>1</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:otherwise>0</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>

	<xsl:variable name="depth.from.context" select="count(ancestor::*)-count($toc-context/ancestor::*)"/>

	<xsl:variable name="subtoc.list">
		<xsl:text>&lt;dd&gt;</xsl:text>
			<xsl:copy-of select="$subtoc"/>
		<xsl:text>&lt;/dd&gt;</xsl:text>
		<xsl:value-of select="$newline"/>
	</xsl:variable>

	<xsl:text>&lt;dt&gt;</xsl:text>
	<xsl:call-template name="toc.line">
		<xsl:with-param name="toc-context" select="$toc-context"/>
	</xsl:call-template>
	<xsl:text>&lt;/dt&gt;</xsl:text>
	<xsl:value-of select="$newline"/>
	
	<xsl:if test="$toc.section.depth > $depth and count($nodes)&gt;0
								and $toc.max.depth > $depth.from.context">
		<xsl:copy-of select="$subtoc.list"/>
	</xsl:if>
</xsl:template>
<!-- ================================================================ -->

</xsl:stylesheet>
