// Restore exact shared public promotional/font assets from the sibling app source.
import {copyFile,readFile} from 'node:fs/promises';import {createHash} from 'node:crypto';
for(const [source,target] of [['../native-client-0.5.0/App/Assets/Landmark.jpg','./sites-candidate/web/landmark.jpg'],['../native-client-0.5.0/App/Assets/Anton-Regular.ttf','./sites-candidate/web/Anton-Regular.ttf']]){
 const input=new URL(source,import.meta.url),output=new URL(target,import.meta.url);await copyFile(input,output);const a=await readFile(input),b=await readFile(output);if(createHash('sha256').update(a).digest('hex')!==createHash('sha256').update(b).digest('hex'))throw Error('Shared source asset copy hash mismatch');
}
console.log('Restored exact shared candidate background/font assets. No game binary or credential access.');
