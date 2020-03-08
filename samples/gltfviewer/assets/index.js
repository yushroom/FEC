import * as fe from 'FishEngine';
import {glTF, LoadglTFFromFile} from './glTFLoader.js'
import * as imgui from 'imgui';
import {assert, print2, LoadFileAsJSON} from './utils.js'

let loaded = new Map();
//let duck = LoadglTFFromFile("D:\\workspace\\glTF-Sample-Models\\2.0\\Duck\\glTF\\Duck.gltf")

let list = [];
let selectedModel = -1;
const models = LoadFileAsJSON("D:\\workspace\\glTF-Sample-Models\\2.0\\model-index.json");
for (const m of models) {
    if ('glTF' in m.variants) {
        list.push({name: m.name, glTF: m.variants.glTF});
    }
}


globalThis.renderUI = ()=>{
    const oldSelected = selectedModel;
    imgui.Begin("glTF-Sample-Models");
    list.forEach((l, index)=>{
        if (imgui.Selectable(l.name, index==selectedModel)) {
            selectedModel = index;
        }
        if (imgui.IsItemFocused()) {
            selectedModel = index;
        }
    });
    imgui.End();

    if (oldSelected != selectedModel)
    {
        // w.Clear();
        fe.reload();
        if (selectedModel >=0 && selectedModel < list.length)
        {
            let duck = loaded.get(selectedModel);
            if (!duck) {
                const {name, glTF} = list[selectedModel];
                const path = `D:\\workspace\\glTF-Sample-Models\\2.0\\${name}\\glTF\\${glTF}`;
                duck = LoadglTFFromFile(path);
                loaded.set(selectedModel, duck);
            }
            duck.Instantiate();
        }
    }
}
