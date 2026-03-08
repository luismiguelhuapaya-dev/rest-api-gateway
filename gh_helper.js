const fs = require('fs');
const path = require('path');
const home = process.env.USERPROFILE || process.env.HOME;
const file = path.join(home, 'gh_tmp.json');
try {
  const j = JSON.parse(fs.readFileSync(file, 'utf8'));
  console.log(JSON.stringify({name: j.full_name, message: j.message, clone_url: j.clone_url, html_url: j.html_url}));
} catch(e) {
  console.log(JSON.stringify({error: e.message}));
}
