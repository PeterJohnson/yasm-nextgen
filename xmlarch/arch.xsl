<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:saxon="http://icl.com/saxon"
	extension-element-prefixes="saxon">

	<xsl:output method="xml" saxon:indent-spaces="2" indent="yes"/>

	<xsl:strip-space elements="*"/>

	<xsl:template match="@*|node()">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:template>

	<xsl:template match="text()">
		<xsl:value-of select="normalize-space(.)"/>
	</xsl:template>

	<xsl:template match="Code/text()">
		<xsl:copy>
			<xsl:apply-templates/>
		</xsl:copy>
	</xsl:template>

	<!--<xsl:template match="comment()"/>-->

</xsl:stylesheet>
<!-- vim: set ts=2 sw=2 sts=2 tw=78 noet: -->
