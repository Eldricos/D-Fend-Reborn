// Help pages script (works in all current browsers; the help is opened in
// the default web browser by D-Fend Reborn)

// shows / hides a collapsible section (FAQ entries etc.)
function Work(id)
{
  var e = document.getElementById(id);
  if (e == null) e = document.getElementsByTagName("div")[id];
  if (e != null) e.style.display = (e.style.display == "") ? "none" : "";
  return false;
}

// link to the contents page at the top of every page (the CHM viewer of the
// original program showed the table of contents next to the pages)
document.addEventListener("DOMContentLoaded", function() {
  var page = location.pathname.substring(location.pathname.lastIndexOf("/") + 1);
  if (page.toLowerCase() == "index.html") return;
  var nav = document.createElement("p");
  nav.className = "nav";
  nav.innerHTML = '<a href="Index.html">&laquo; Inhalt</a>';
  document.body.insertBefore(nav, document.body.firstChild);
});
