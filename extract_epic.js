const fs = require('fs');
const path = require('path');
const data = JSON.parse(fs.readFileSync(path.join(process.env.HOME || process.env.USERPROFILE, 'rapig1.json'),'utf8'));
const f = data.fields;
console.log('Key:', data.key);
console.log('Summary:', f.summary);
console.log('Status:', f.status.name);
console.log('Type:', f.issuetype.name);
console.log();

function extractText(node) {
  if (!node) return '';
  if (typeof node === 'string') return node;
  if (Array.isArray(node)) return node.map(extractText).join(' ');
  if (node.type === 'text') return node.text || '';
  if (node.type === 'heading') return '\n## ' + (node.content||[]).map(extractText).join('') + '\n';
  if (node.type === 'bulletList') return (node.content||[]).map(c => '- ' + extractText(c)).join('\n');
  if (node.type === 'orderedList') return (node.content||[]).map((c,i) => (i+1)+'. ' + extractText(c)).join('\n');
  if (node.type === 'listItem') return (node.content||[]).map(extractText).join(' ');
  if (node.type === 'codeBlock') return '\n```\n' + (node.content||[]).map(extractText).join('') + '\n```\n';
  if (node.type === 'table') {
    return (node.content||[]).map(r => (r.content||[]).map(cell => (cell.content||[]).map(extractText).join(' ')).join(' | ')).join('\n');
  }
  if (node.type === 'rule') return '\n---\n';
  return (node.content||[]).map(extractText).join(' ');
}

console.log(extractText(f.description));
