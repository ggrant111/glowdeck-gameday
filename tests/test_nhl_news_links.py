"""Regression: provider links must never route ESPN IDs through NHL.com."""
from pathlib import Path
import subprocess

source=Path("include/WebUi.h").read_text(encoding="utf-8")
start=source.index("function newsArticleDestination(article){")
end=source.index("async function loadNews(){",start)
helper=source[start:end]
script=helper+"""
const assert=require("assert");
const check=newsArticleDestination({
  slug:"espn-50477832",
  source:"NHL", // even stale/wrong source metadata must be safe
  url:"https://www.nhl.com/news/espn-50477832"
});
assert(check && check.href==="https://www.espn.com/nhl/story/_/id/50477832");
assert(check.label==="Read on ESPN");
assert(!check.href.includes("nhl.com/news/espn"));
const canonical=newsArticleDestination({
  slug:"espn-50477832",
  source:"ESPN",
  url:"https://www.espn.com/nhl/story/_/id/50477832/existing-editorial-title"
});
assert(canonical.href==="https://www.espn.com/nhl/story/_/id/50477832/existing-editorial-title");
const trueNhl=newsArticleDestination({
  slug:"mammoth-game-recap-november-13",
  source:"NHL",
  url:"https://www.nhl.com/news/mammoth-game-recap-november-13"
});
assert(trueNhl.href==="https://www.nhl.com/news/mammoth-game-recap-november-13");
const forged=newsArticleDestination({
  slug:"espn-50477832",
  source:"ESPN",
  url:"https://www.espn.com.evil.test/article"
});
assert(forged.href==="https://www.espn.com/nhl/story/_/id/50477832");
assert(newsArticleDestination({slug:"",source:"ESPN",url:"https://www.nhl.com/news/example"})===null);
console.log("PASS: ESPN IDs never generate NHL links; publisher canonical URLs preserved");
"""
subprocess.run(["node","-e",script],check=True)
