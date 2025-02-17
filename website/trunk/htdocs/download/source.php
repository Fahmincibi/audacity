<?php
/*
 * Copyright 2004 - 2014
 * Matt Brubeck
 * Dominic Mazzoni
 * Richard Ash
 * Gale Andrews 
 * Vaughan Johnson
 * This file is licensed under a Creative Commons license:
 * http://creativecommons.org/licenses/by/3.0/
 */
  require_once "main.inc.php";
  require_once "../latest/versions.inc.php";
  require_once "../latest/mirror.inc.php";
  $pageId = "source";
  $pageTitle = _("Source Code");
  include "../include/header.inc.php";
?>

<h2><?=$pageTitle?></h2>

<ul>
  <li><p><?php printf(_('<a href="%s">Audacity %s source tarball</a> (%s file, %.1lf MB)'), download_url($min_src_url), src_version, src_suffix, min_src_size)?> 
<?php printf(_(': Standard source tarball, primarily for GNU/Linux. This assumes your system has necessary dependencies and libraries installed.'))?></p></li>

  <li><p><?php printf(_('<a href="%s">Git Repository</a>&nbsp;'), "../community/developers#git")?>
<?php printf(_('For all platforms. Check out either the latest Audacity development code or a specific tagged release. Git checkouts are suitable for any system, including those that lack necessary dependencies to compile the standard source tarball.'))?>
</ul>

<h3 id="sysreq"><?=_("System Requirements")?></h3>
 <ul>
  <li><a href="./windows#sysreq">Windows</a>, <a href="./mac#sysreq">Mac</a>, or <a href="./linux#sysreq">GNU/Linux</a></li>
 </ul>


<h3 id="instructions"><?=_("How to Compile Audacity")?></h3>

<h4><?=_("Dependencies")?></h4>
<p><?php printf(_('The <a href="http://wxwidgets.org">wxWidgets</a> library is required. Audacity %s requires wxGTK 2.8.12. The <a href="http://www.mega-nerd.com/libsndfile/">libsndfile</a> library is also required and is included in Audacity obtained from <a href="/community/developers#git">Git</a>. Installation of other libraries is <a href="http://wiki.audacityteam.org/wiki/Developing_On_Linux#Optional_Packages_and_Features">optional</a>.'), src_version)?></p>

<p><?php printf(_('<a href="%s">CMake</a> is required to build <a href="%s">libsoxr</a> which is now the Audacity default resampling library.'), "http://www.cmake.org/", "http://sourceforge.net/p/soxr/wiki/Home/")?></p>

<p><?=_('If you install libraries using a package management system like Apt or RPM, you need to install the "dev" (development) packages for each library.')?></p>

<h4><?=_("Compilation")?></h4>
<p><?=_("To compile Audacity, run the following command in the Audacity source directory:")?></p>
<blockquote>
<i>./configure &amp;&amp; make</i>
</blockquote>
<p><?=_("You can type <i>./configure --help</i> to see a list of compilation options. After Audacity is compiled, run <i>make install</i> as root to install it.")?></p>

<p><?php printf(_('Those new to compiling are recommended to look at %sCompiling Audacity for Beginners%s. Information for Developers is at %sDeveloper Guide%s.'), '<a href="http://wiki.audacityteam.org/wiki/CompilingAudacityForBeginners">', '</a>', '<a href="http://wiki.audacityteam.org/wiki/Developer_Guide">', '</a>')?></p>

<p>&nbsp;</p>

<?php
  include "../include/footer.inc.php";
?>