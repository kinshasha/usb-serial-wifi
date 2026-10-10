const fs=require('fs'), vm=require('vm'), assert=require('assert');
const source=fs.readFileSync('include/web_print_page.h','utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
const calls=[], elements={}, buttons=[{},{}];
for(const id of ['result','text','file','fileForm','textForm']) elements[id]={value:'',files:[],addEventListener(type,fn){this.handler=fn;}};
const context=vm.createContext({document:{getElementById:id=>elements[id],querySelectorAll:()=>buttons},TextEncoder,Uint8Array,fetch:async(url,options)=>{calls.push({url,options});return{text:async()=> 'Print job queued',ok:true};}});
vm.runInContext(script,context);
(async()=>{
 elements.text.value='Hello\n';
 elements.textForm.handler({preventDefault(){}});
 await new Promise(setImmediate);
 assert.equal(calls.length,1); assert.equal(calls[0].url,'/print');
 assert.equal(calls[0].options.headers['Content-Type'],'text/plain; charset=utf-8');
 assert.equal(Buffer.from(calls[0].options.body).toString(),'Hello\n');
 elements.file.files=[{size:3,arrayBuffer:async()=>Uint8Array.from([65,0,255]).buffer}];
 await elements.fileForm.handler({preventDefault(){}});
 assert.equal(calls.length,2); assert.equal(calls[1].options.headers['Content-Type'],'application/octet-stream');
 assert.deepEqual(Array.from(calls[1].options.body),[65,0,255]); // Device does filtering.
 elements.text.value='x'.repeat(8193); elements.textForm.handler({preventDefault(){}});
 await new Promise(setImmediate); assert.equal(calls.length,2);
 elements.text.value=''; elements.textForm.handler({preventDefault(){}});
 await new Promise(setImmediate); assert.equal(calls.length,2);
 context.fetch=async()=>{throw Error('lost');}; elements.text.value='Hello';
 elements.textForm.handler({preventDefault(){}}); await new Promise(setImmediate);
 assert.match(elements.result.textContent,/unknown/); assert(buttons.every(b=>b.disabled===false));
 console.log('File/text submission, size limits and uncertain-delivery UI tests passed');
})().catch(error=>{console.error(error);process.exit(1);});
