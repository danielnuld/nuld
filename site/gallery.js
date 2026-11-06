// Screens: the strip scrolls with its buttons (or a swipe, it is a scroll-snap row); a screenshot opens the viewer,
// which goes through them all with its buttons, the arrow keys or a swipe. Without JavaScript the strip still scrolls.
(function () {
  var strip = document.querySelector("[data-strip]"), viewer = document.querySelector("[data-viewer]");
  if (!strip || !viewer) return;
  var shots = Array.prototype.slice.call(strip.querySelectorAll("figure"));
  var img = viewer.querySelector("img"), cap = viewer.querySelector("[data-cap]"), count = viewer.querySelector("[data-count]");
  var at = 0;

  function step(d) { // the strip: one screenshot left or right
    var w = shots[0].getBoundingClientRect().width + 20;
    strip.scrollBy({ left: d * w, behavior: matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth" });
  }
  document.querySelectorAll("[data-strip-step]").forEach(function (b) {
    b.addEventListener("click", function () { step(+b.getAttribute("data-strip-step")); });
  });

  function show(i) {
    at = (i + shots.length) % shots.length;
    var s = shots[at].querySelector("img");
    img.src = s.src;
    img.alt = s.alt;
    cap.textContent = shots[at].querySelector("figcaption").textContent;
    count.textContent = (at + 1) + " / " + shots.length;
  }
  shots.forEach(function (f, i) {
    f.querySelector("button").addEventListener("click", function () { show(i); viewer.showModal(); });
  });
  viewer.querySelectorAll("[data-go]").forEach(function (b) {
    b.addEventListener("click", function () { show(at + +b.getAttribute("data-go")); });
  });
  viewer.querySelector("[data-close]").addEventListener("click", function () { viewer.close(); });
  viewer.addEventListener("keydown", function (e) {
    if (e.key === "ArrowRight") show(at + 1), e.preventDefault();
    if (e.key === "ArrowLeft") show(at - 1), e.preventDefault();
  });
  viewer.addEventListener("click", function (e) { if (e.target === viewer || e.target.classList.contains("stage")) viewer.close(); });
  var x0 = null; // swipe
  viewer.addEventListener("pointerdown", function (e) { x0 = e.clientX; });
  viewer.addEventListener("pointerup", function (e) {
    if (x0 !== null && Math.abs(e.clientX - x0) > 50) show(at + (e.clientX < x0 ? 1 : -1));
    x0 = null;
  });
  viewer.addEventListener("close", function () { shots[at].querySelector("button").focus(); });
})();
