#pragma once
// Upload raw bytes rather than multipart framing. The firmware performs the
// filtering; the browser only provides the two submission mechanisms.
static const char WEB_PRINT_PAGE[] = R"HTML(<!doctype html>
<html lang="en"><meta name="viewport" content="width=device-width"><meta charset="utf-8">
<title>Print text</title><style>body{font:17px system-ui;max-width:700px;margin:30px auto;padding:16px}textarea{width:100%;box-sizing:border-box;min-height:220px;font:16px monospace}button,input{font:inherit;margin:8px 0;padding:8px}#result{white-space:pre-wrap}button:disabled{opacity:.5}</style>
<h1>Print text</h1><p><a href="/">Home</a> · <a href="/status">Status</a></p>
<p>Keep the printer selected and in USB Printer Class mode. Choose a plain text file or paste text below. Printable ASCII and line breaks are kept; other bytes are removed. Tabs become spaces.</p>
<p>Maximum 8 KiB before and after filtering. PDF, Word and image files are not converted. No form feed is added.</p>
<form id="fileForm"><h2>Upload a text file</h2><input id="file" type="file" accept=".txt,.csv,.log,text/plain" required><br><button type="submit">Upload and print file</button></form>
<form id="textForm"><h2>Paste text</h2><textarea id="text" aria-label="Text to print" required></textarea><br><button type="submit">Print pasted text</button></form>
<p id="result" role="status" aria-live="polite"></p>
<script>
const result=document.getElementById('result');
async function submit(data,type){
 if(!data.byteLength){result.textContent='Choose a nonempty file or enter text.';return;}
 if(data.byteLength>8192){result.textContent='Submission exceeds 8 KiB. Choose a smaller file or text.';return;}
 const buttons=document.querySelectorAll('button');buttons.forEach(b=>b.disabled=true);
 result.textContent='Submitting…';
 try{const response=await fetch('/print',{method:'POST',headers:{'Content-Type':type},body:data});result.textContent=await response.text();}
 catch(error){result.textContent='Connection interrupted. Delivery is unknown; check printer and Status before submitting again.';}
 finally{buttons.forEach(b=>b.disabled=false);}
}
document.getElementById('textForm').addEventListener('submit',event=>{event.preventDefault();submit(new TextEncoder().encode(document.getElementById('text').value),'text/plain; charset=utf-8');});
document.getElementById('fileForm').addEventListener('submit',async event=>{event.preventDefault();const file=document.getElementById('file').files[0];if(!file)return;if(file.size>8192){result.textContent='File exceeds 8 KiB.';return;}try{await submit(new Uint8Array(await file.arrayBuffer()),'application/octet-stream');}catch(error){result.textContent='Could not read the selected file.';}});
</script></html>)HTML";
