<?php
/*
 * Copyright 2004 Matt Brubeck
 * 2007-2015 Vaughan Johnson, Gale Andrews
 * This file is licensed under a Creative Commons license:
 * http://creativecommons.org/licenses/by/3.0/
 */
  require_once "main.inc.php";
  $pageId = "developers";
  $pageTitle = _("Developers");
  include "../include/header.inc.php";
?>

<h2><?=$pageTitle?></h2>

<p><?=_('<a href="https://www.openhub.net/p/audacity">Open HUB</a> (formerly known as "Ohloh") has statistics on the value of Audacity development. The Open HUB &quot;badge&quot; at the bottom of each page on this site shows updated summary values. The statistics are set up by us to show only the values of Audacity-specific development, excluding the third-party code libraries Audacity uses. Searchable <a href="https://www.openhub.net/p/audacity/commits">commit listings</a> are produced every 10 days.')?></p>

<h3><?=_('Mailing Lists')?></h3>
<p><?=_('Join the <a href="http://lists.sourceforge.net/lists/listinfo/audacity-devel">audacity-devel</a> mailing list to discuss current and future development, or to get help modifying the Audacity source code.')?></p>

<h3 id="git"><?=_('Git')?></h3>
<p><?=_('You can get the latest Audacity code from our <a href="https://github.com/audacity/audacity">Git repository</a>.')?>

<ul>
  <li><p><?=_('Type this at the command line to make a local copy of the source code repository from GitHub:')?></p>
  <p><kbd>git clone https://github.com/audacity/audacity</kbd></p>
</ul>
</p>

<p><?php printf(_('Our Git repository has an <a href="https://github.com/audacity/audacity/commits/master.atom">atom feed</a>. You can also look at our Git <a href="https://github.com/audacity/audacity/network">work in progress</a>.'))?>
</p>


<h3><?=_("Compiling Audacity")?></h3>
<p><?php printf('<ul><li>%s</li>',
	_('On Windows, see the file "compile.txt" inside the "Win" folder in the source code. For OS X, see "compile.txt" inside the "Mac" folder in the code.'));
  printf(_('%s Further information can be found in <a href="%s">Developer Guide</a> and in our more detailed guides on compiling Audacity for %sWindows%s, %sMac%s and %sGNU/Linux%s.%s'),
   '<li>',
   'http://wiki.audacityteam.org/wiki/Developer_Guide',
   '<a href="http://wiki.audacityteam.org/wiki/Developing_On_Windows">',
   '</a>',
   '<a href="http://wiki.audacityteam.org/wiki/Building_On_Mac">',
   '</a>',
   '<a href="http://wiki.audacityteam.org/wiki/Developing_On_Linux">',
   '</a>',
   '</li>');
  printf(_('%sIf you are still having difficulties, please ask on the %sCompiling Audacity%s board on the %sAudacity Forum%s.%s'),
  '<li>',
  '<a href="http://forum.audacityteam.org/viewforum.php?f=19">',
  '</a>',
  '<a href="http://forum.audacityteam.org">',
  '</a>',
  '</li></ul>');
?></p>

<h3 id="bugs"><?=_('Bug Tracking')?></h3>
<p><?=_('The Audacity developers use bugzilla to track bugs and enhancements. To find issues to work on, please view our categorized <a href="http://wiki.audacityteam.org/wiki/Bug_Lists">Bug Lists</a>.')?></p>
<p><?=_('If you are a user reporting a new bug, please email our <a href="../contact#feedback">feedback address</a>.')?></p>

<h3><?=_('Git Pull Requests and Patches')?></h3>
<p><?=_('We welcome Git pull requests from developers. Also see <a href="http://wiki.audacityteam.org/wiki/SubmittingPatches">Submitting Patches</a> on the <a href="http://wiki.audacityteam.org/wiki">Audacity Wiki</a> for an alternative way of offering us code improvements.')?></p>

<?php
  include "../include/footer.inc.php";
?>
