<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0"
xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:b="Broadcom">

<xsl:output method="html"
            encoding="UTF-8"
            indent="no"/>
	
<xsl:template match="/">
  <html>
  
  <head>	
	<link rel="stylesheet" type="text/css" href="/diff/diffview.css"/>
	<script type="text/javascript" src="/diff/diffview.js" defer="defer"></script>
	<script type="text/javascript" src="/diff/difflib.js" defer="defer"></script>
	
<script type="text/javascript" defer="defer">

<![CDATA[
function diffUsingJS(viewType, baseTitle, baseText, newTitle, newText, diffOutputId) {
	"use strict";
	var byId = function (id) { return document.getElementById(id); },
		base = difflib.stringAsLines(byId(baseText).textContent),
		newtxt = difflib.stringAsLines(byId(newText).textContent),
		sm = new difflib.SequenceMatcher(base, newtxt),
		opcodes = sm.get_opcodes(),
		diffoutputdiv = byId(diffOutputId);	

	diffoutputdiv.innerHTML = "";	

	diffoutputdiv.appendChild(diffview.buildView({
		baseTextLines: base,
		newTextLines: newtxt,
		opcodes: opcodes,
		baseTextName: baseTitle,
		newTextName: newTitle,
		contextSize: null,
		viewType: viewType
	}));
}
 ]]>
 
</script>
</head>

  <body>
  <h2>GRC Analysis</h2>
  <xsl:apply-templates select="b:BroadcomDesign"/>
  </body>
  </html>
</xsl:template>

<xsl:template match="b:Path">
  <hr />
  <h3>Path = <xsl:value-of select="@num"/></h3>
	<ul style="list-style-type: none;">
		<xsl:apply-templates/>
	</ul>	
</xsl:template>

<xsl:template match="b:Errors">    
  <xsl:if test="b:error">
	<h3>Errors</h3>
	<table border="1">
			<tr bgcolor="red">
				<xsl:for-each select="b:error[1]/@*">		
					<th><xsl:value-of select="name()"/> </th>
				</xsl:for-each>
			</tr>
			<xsl:for-each select="b:error">		
				<tr>			
					<xsl:for-each select="@*">		
						<td><xsl:value-of select="."/></td> 
					</xsl:for-each>
				</tr>
			</xsl:for-each>
		</table>
	</xsl:if>
</xsl:template>

  <xsl:template match="b:Error-If-No-Traffic">
    <xsl:if test="b:error">
      <h3>Error-If-No-Traffic</h3>
      <table border="1">
        <tr bgcolor="orange">
          <xsl:for-each select="b:error[1]/@*">
            <th>
              <xsl:value-of select="name()"/>
            </th>
          </xsl:for-each>
        </tr>
        <xsl:for-each select="b:error">
          <tr>
            <xsl:for-each select="@*">
              <td>
                <xsl:value-of select="."/>
              </td>
            </xsl:for-each>
          </tr>
        </xsl:for-each>
      </table>
    </xsl:if>
  </xsl:template>

  <xsl:template match="b:Warnings">
    <xsl:if test="b:warning">
      <h3>Warnings</h3>
      <table border="1">
        <tr bgcolor="yellow">
          <xsl:for-each select="b:warning[1]/@*">
            <th>
              <xsl:value-of select="name()"/>
            </th>
          </xsl:for-each>
        </tr>
        <xsl:for-each select="b:warning">
          <tr>
            <xsl:for-each select="@*">
              <td>
                <xsl:value-of select="."/>
              </td>
            </xsl:for-each>
          </tr>
        </xsl:for-each>
      </table>
    </xsl:if>
  </xsl:template>


  <xsl:template match="b:section">
	<li>
				<div>
					<ul style="list-style-type: none;">
						<xsl:choose>
							<xsl:when test="@name">
								<h3 class="section section-open" style="cursor: pointer;">
									<span class="ui-icon ui-icon-triangle-1-s"></span>
									<xsl:value-of select="@name"/>
								</h3>
								<div class="data">
									<xsl:apply-templates/>
								</div>
							</xsl:when>
							<xsl:otherwise>
								<xsl:apply-templates/>
							</xsl:otherwise>
						</xsl:choose>
					</ul>
				</div>
	</li>
</xsl:template>

<xsl:template match="b:subsection">
	<div>
		<h4 class="section section-closed" style="cursor: pointer;">
			<span class="ui-icon ui-icon-triangle-1-e"></span><xsl:value-of select="@name"/>
		</h4>
		<div class="data">
		<xsl:apply-templates/>
		</div>
	</div>
</xsl:template>

<xsl:template match="b:table">
	<li>
		<table border="1">
			<tr bgcolor="#aaaaaa">
				<xsl:for-each select="b:row[1]/b:elem">
					<th>
						<xsl:value-of select="@name"/>
					</th>
				</xsl:for-each>
			</tr>
			<xsl:apply-templates/>
		</table>
	</li>	
</xsl:template>

<xsl:template match="b:row">
	<tr>
		<xsl:apply-templates/>
	</tr>
</xsl:template>

<xsl:template match="b:elem">
	<td>
		<!--xsl:value-of select="."/-->
		<xsl:call-template name="insertBreaks">
			<xsl:with-param name="pText" select="."/>
		</xsl:call-template>
	</td>
</xsl:template>
	
<xsl:template match="b:item">
	<table border="1">
		<tr bgcolor="orange">
			<xsl:for-each select="@*">		
				<th><xsl:value-of select="name()"/></th>
			</xsl:for-each>
		</tr>
		<tr>
			<xsl:for-each select="@*">		
				<!--td><xsl:value-of select="."/></td-->
				<td>
					<!--xsl:apply-templates select="."/-->					
					<xsl:call-template name="insertBreaks">
						<xsl:with-param name="pText" select="."/>
				   </xsl:call-template>
				</td>
			</xsl:for-each>
		</tr>
	</table>
	<dd>
		<table border="1">
			<tr bgcolor="#9acd32">
				<xsl:for-each select="b:item[1]/@*">		
					<th><xsl:value-of select="name()"/> </th>
				</xsl:for-each>
			</tr>
			<xsl:for-each select="b:item">		
				<tr>			
					<xsl:for-each select="@*">		
						<td><xsl:value-of select="."/></td> 
					</xsl:for-each>
				</tr>
			</xsl:for-each>
	</table>
	</dd>
	<!--xsl:apply-templates/-->	
</xsl:template>


<xsl:template match="b:diff">
	
	<xsl:variable name="diff-base-id" select="concat('diff-base-id-', generate-id(b:base))"/>
	<xsl:variable name="diff-new-id" select="concat('diff-new-id-', generate-id(b:new))"/>
	<xsl:variable name="diff-output-id" select="concat('diff-output-id-', generate-id())"/>
	<xsl:variable name="diff-func" select="concat('diff_', generate-id())"/>
	
	<script type="text/javascript" defer="defer">	
	
		function <xsl:value-of select='$diff-func'/>(viewType) {	
			diffUsingJS(viewType, "<xsl:value-of 
				select='b:base/@name'/>", "<xsl:value-of 
				select='$diff-base-id'/>", "<xsl:value-of 
				select='b:new/@name'/>", "<xsl:value-of 
				select='$diff-new-id'/>", "<xsl:value-of 
				select='$diff-output-id'/>");
		}
	
	</script>
	
	<div id="{$diff-base-id}" style="display:none;">
		<xsl:value-of select='b:base'/>
	</div>
	
	<div id="{$diff-new-id}" style="display:none;">
		<xsl:value-of select='b:new'/>
	</div>
	
	<div>
		<input type="radio" name="{$diff-func}" id="{$diff-func}_sidebyside" onclick="{$diff-func}(0);" /> <label for="{$diff-func}_sidebyside">Side by Side Diff</label>
		<input type="radio" name="{$diff-func}" id="{$diff-func}_inline" onclick="{$diff-func}(1);" /> <label for="{$diff-func}_inline">Inline Diff</label>
	</div>
	
	<div id="{$diff-output-id}" style="width: 100%;" > </div>
	
	<script type="text/javascript" defer="defer">		
		<xsl:value-of select='$diff-func'/>(1);
	</script>
	
</xsl:template>

<xsl:template name="insertBreaks">
   <xsl:param name="pText"/>
   <xsl:choose>
     <xsl:when test="not(contains($pText, '\n'))">		
       <xsl:value-of select="$pText"/>	   
     </xsl:when>
     <xsl:otherwise>
       <xsl:value-of select="substring-before($pText, '\n')"/>	  
       <br />
       <xsl:call-template name="insertBreaks">
         <xsl:with-param name="pText" select=
           "substring-after($pText, '\n')"/>
       </xsl:call-template>
     </xsl:otherwise>
   </xsl:choose>
 </xsl:template>

</xsl:stylesheet>
