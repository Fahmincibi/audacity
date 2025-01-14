<?php
/*
 * Copyright 2004 - 2015 Matt Brubeck
 * This file is licensed under a Creative Commons license:
 * http://creativecommons.org/licenses/by/3.0/
 */
  require_once "main.inc.php";
  $pageTitle = _("Website Copyright and Trademark");
  include "include/header.inc.php";
?>

<h2><?=$pageTitle?></h2>

<?php
  $thisYear = date("Y");

  // i18n-hint: The Creative Commons web page below is available in several
  // languages.  Please link to the version in your language if possible.
	//
	// You may add a sentence like "Czech translation copyright YOUR NAME HERE"
	// if you wish.
  echo "<p>".sprintf(_('This website is Copyright &copy; %s members of the Audacity development team.  Except where otherwise noted, all text and images on this site are licensed under the <a href="http://creativecommons.org/licenses/by/3.0/">Creative Commons Attribution License, version 3.0</a>.  You may modify, copy, distribute, and display this material, but you must give credit to the original authors.  Please see the license for details.'), $thisYear)."</p>";

  echo "<p>"._('"Audacity" is a trademark of Dominic Mazzoni.  The Google logo is a trademark of Google, Inc.')."</p>";

  include "include/footer.inc.php";
?>
