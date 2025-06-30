// place files you want to import through the `$lib` alias in this folder.


// place files you want to import through the `$lib` alias in this folder.

import {PUBLIC_API_PATH} from "$env/static/public";



export type Device = {
    name: string | undefined
    connected: boolean | undefined
    rssi: number | undefined
    connections_24h: number | undefined
    last_connect: number | undefined
    last_disconnect: number | undefined    
    devkey: string | undefined
}






let session = {hash: ""}

export function saveSession(h:string){
    localStorage.setItem("session", h)
    session.hash = h
    console.log("saved session %s", h)
}

function loadSession(){

    if(session.hash.length > 2)
        return session.hash

    let h = localStorage.getItem("session");
    if(h && h.length > 2){
        console.log("loaded session %s", h)
        session.hash = h;
        return h;
    }

    return ""
}


export async function apifetch(path: string, data: {}){

    //@ts-ignore
    if(!data.sesskey || !data.sesskey.length)
        //@ts-ignore
        data.sesskey = loadSession()
        
    return handleResp(
        await fetch(PUBLIC_API_PATH+path, {
            method: "post",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify(data)
        })
    )

    // if(session.hash.length < 2)
    //     loadSession()
    // return handleResp(
    //     await fetch(PUBLIC_API_PATH+path+"?s="+session.hash, {
    //         method: "post",
    //         headers: {
    //             "Content-Type": "application/json"
    //         },
    //         body: JSON.stringify(data)
    //     })
    // )
}

async function handleResp(resp: Response){


    if(resp.status==401){
        if(!window.location.pathname.includes("/login")){
            window.location.href = "./login"
        }
    }

    let data = await resp.json()
    if(data){
        if(data.error && data.error.length>1)
            throw new Error(data.error)
        return data
    }
    throw new Error("failed to load json from response");
    // if(resp.status==200){
    //     let data = await resp.json()
    //     if(data)
    //         return data
    //     throw new Error("failed to load json from response");
    // }
    // else if(resp.status==401){
    //     console.error("server responded 401, not logged in?")
    //     if(window.location.pathname != "/login"){
    //         console.log("redirecting to /home")
    //         window.location.href = "/login"
    //     }
    //     throw new Error("fetch failed, user not logged in");
    // }
    // else{
    //     throw new Error("fetch failed, invalid resp code "+resp.status);
    // }
}

