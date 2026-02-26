<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml"
              omit-xml-declaration="yes"/>
  <xsl:template match="/">
    <xsl:for-each select="Files/File">
      <a>
        <xsl:attribute name="href">
          #<xsl:value-of select="position()"/>
        </xsl:attribute>
      </a>
      <xsl:value-of select="File"/>
      <br/>
    </xsl:for-each>
    <br/>
    <hr size="4" color="#BEBEBE"/>
    <br/>
    <xsl:for-each select="Files/File">
      <p>
        <a>
          <xsl:attribute name="name">
            <xsl:value-of select="position()"/>
          </xsl:attribute>
        </a>
      </p>
      <br/>
      File:
      <xsl:value-of select="File" />
      <br/>
      File size:
      <xsl:value-of select="FileSize" />
      <br/>
      Date modified:
      <xsl:value-of select="DateModified" />
      <br />
      <br/>
      Error message:
      <xsl:value-of select="ErrorMessage" />
      <br />
      <br/>
      <TABLE border="1" width="100%" cellspacing="0">
        <tr align="center">
          <td colspan="3">
            Hashes
          </td>
        </tr>
        <tr>
          <td>
            <span>
              HashAlgorithm
            </span>
          </td>
          <td>
            <span>
              Value
            </span>
          </td>
          <td>
            <span>
              Notes
            </span>
          </td>
        </tr>
        <xsl:apply-templates select="Hashes/Hash" />
      </TABLE>
      <br/>
      <TABLE border="1" width="100%" cellspacing="0">
        <tr align="center">
          <td colspan="16">
            Sessions
          </td>
        </tr>
        <tr>
          <td rowspan="2">
            Id
          </td>
          <td rowspan="2">
            Certificate name
          </td>
          <td colspan="3">
            User
          </td>
          <td colspan="4">
            Environment
          </td>
          <td colspan="3">
            FileInfo
          </td>
          <td rowspan="2">
            RequestTime
          </td>
          <td rowspan="2">
            <p>
            </p>
            SignTime
          </td>
          <td rowspan="2">
            SessionStart
          </td>
          <td rowspan="2">
            Message
          </td>
        </tr>
        <tr>
          <td>UserIDSID</td>
          <td>HostIp</td>
          <td>HostName</td>
          <td>ClientVersion</td>
          <td>OS</td>
          <td>OSArchitecture</td>
          <td>OSVersion</td>
          <td>FileName</td>
          <td>FileTime</td>
          <td>FileSize</td>
        </tr>
        <xsl:apply-templates select="Sessions/Session" />
      </TABLE>
      <br/>
      <hr size="4" color="#BEBEBE"/>
      <br/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="Session">
    <tr>
      <td>
        <xsl:value-of select="Id" />
      </td>
      <td>
        <xsl:value-of select="CertificateName" />
      </td>
      <td>
        <xsl:value-of select="User/UserIDSID" />
      </td>
      <td>
        <xsl:value-of select="User/HostIp" />
      </td>
      <td>
        <xsl:value-of select="User/HostName" />
      </td>
      <td>
        <xsl:value-of select="Environment/ClientVersion" />
      </td>
      <td>
        <xsl:value-of select="Environment/OS" />
      </td>
      <td>
        <xsl:value-of select="Environment/OSArchitecture" />
      </td>
      <td>
        <xsl:value-of select="Environment/OSVersion" />
      </td>
      <td>
        <xsl:value-of select="FileInfo/FileName" />
      </td>
      <td>
        <xsl:value-of select="FileInfo/FileTime" />
      </td>
      <td>
        <xsl:value-of select="FileInfo/FileSize" />
      </td>
      <td>
        <xsl:value-of select="RequestTime" />
      </td>
      <td>
        <xsl:value-of select="SignTime" />
      </td>
      <td>
        <xsl:value-of select="SessionStart" />
      </td>
      <td>
        <xsl:value-of select="Message" />
      </td>
    </tr>
  </xsl:template>
  <xsl:template match="Hash">
    <tr>
      <td>
        <SPAN>
          <xsl:value-of select="HashAlgorithm" />
        </SPAN>
      </td>
      <td>
        <SPAN>
          <code>
            <xsl:value-of select="Value" />
          </code>
        </SPAN>
      </td>
      <td>
        <SPAN>
          <xsl:value-of select="Notes" />
        </SPAN>
      </td>
    </tr>
  </xsl:template>
</xsl:stylesheet>