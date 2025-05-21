CREATE TABLE IF NOT EXISTS SchemaVersion (
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    applied_on DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS Project (
    id TEXT PRIMARY KEY CHECK(LENGTH(id) = 36),
    title TEXT NOT NULL,
    description TEXT NOT NULL,
    tags TEXT NOT NULL,
    imageUrl TEXT NOT NULL,
    githubUrl TEXT NOT NULL,
    websiteUrl TEXT DEFAULT NULL,
    githubReadme TEXT DEFAULT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO Project (id, title, description, imageUrl, tags, githubUrl, websiteUrl, githubReadme) VALUES 
('066691ec-2b6c-4799-b802-af92c989e84c', 'Kubernetes manifest files', 'A project containing all my Kubernetes manifest files I write to deploy my applications and database to my Kubernetes cluster. Additionally, it contains most configuration files for external plugins I have added to the cluster. All files are written in YAML format.', 'https://yafxgqyvfrhuqxefmwvc.supabase.co/storage/v1/object/public/images/public/066691ec-2b6c-4799-b802-af92c989e84c/Manifest.png', 'Database,ArgoCD,Grafana,Ingress,Kubernetes,YAML', 'https://github.com/diderikk/manifests/blob/main/encryption-config.yaml', null, null), 
('55ba6e88-4d8f-4713-ae15-83d9d19e81f2', 'Chess web application', '

This hobby project was developed during the summer 2022 concurrently with a summer intership (not part of). The main objective was to experiment with Elixir/OTP. As a result I developed a full stack application using Elixir/Phoenix (API framework) and Svelte (JS framework). The backend consists of a pure websocket interface (no HTTP endpoints, only WS endpoints) and no database. All data is stored in memory using Elixir/OTP. Elixir/OTP are independent processes with memory that can communicate with the application.
Svelte was selected as the UI framework because of its reputation of being lightweight, simple, has Typescript support and was a technology that I wanted to experiment with.', 'https://yafxgqyvfrhuqxefmwvc.supabase.co/storage/v1/object/public/images/public/55ba6e88-4d8f-4713-ae15-83d9d19e81f2/Chess.png', 'WebSocket,OTP,Phoenix,Elixir,Svelte,Phoenix', 'https://github.com/diderikk/Chess', 'https://chess.diderikk.dev', 'https://raw.githubusercontent.com/diderikk/Chess/master/'), 
('7730319e-38ec-4547-87aa-464498af92a7', 'SimpleChat web application', 'SimpleChatApp is a full-stack application that uses Elixir/Phoenix as an API framework. This app allows users to sign up, sign in, create new chats, and invite other users. However, there is currently no method for inviting users to existing chats, which needs to be implemented. Chatting is possible through Phoenix Channels and WebSockets, and users must be authorized to enter a chat.', 'https://yafxgqyvfrhuqxefmwvc.supabase.co/storage/v1/object/public/images/public/7730319e-38ec-4547-87aa-464498af92a7/SimpleChat.png', 'WebSocket,ChakraUI,TypeScript,React,Phoenix,Elixir', 'https://github.com/diderikk/SimpleChatApp', 'https://chat.diderikk.dev', 'https://raw.githubusercontent.com/diderikk/SimpleChatApp/master/'), 
('af583797-e988-4f2f-b7dd-b598e47c8ec1', 'IssueBoard web application', 'IssueBoard is a web application made to store issues for projects in boards. Users are able to create their own boards or join groups for collaborative work. All members of the group are able to add new issue boards and invite new members to that group. A user is also able to add members to a non group issue board that they own. Issue board functionality described later.

The web application is made with React, this is developers first time using this framework. It was selected for it''s popularity and online support. For styling, the app is made with solely CSS, and not with a CSS-framework. For the backend part of the application, Ruby on Rails is being used.', 'https://yafxgqyvfrhuqxefmwvc.supabase.co/storage/v1/object/public/images/public/af583797-e988-4f2f-b7dd-b598e47c8ec1/IssueBoard.png', 'GraphQL,JWT,TypeScript,React,Ruby,Rails', 'https://github.com/diderikk/IssueBoard', 'https://issueboard.diderikk.dev', 'https://raw.githubusercontent.com/diderikk/IssueBoard/main/'), 
('c12b5210-afc7-404a-9d66-a2245408456e', 'Portfolio v1 (decommisioned)', 'Portfolio''s primary purpose is to showcase all of my notable projects and personal notes in one convenient location. It consists of mainly frontend websites and an API to publish new projects or project notes. The API is hidden behind two BasicAuth authentications, one for entering the form page and one for posting the form to the API. For storage, Supabase is used to serialize both images and objects. This application was crafted using the NextJS framework. ', 'https://yafxgqyvfrhuqxefmwvc.supabase.co/storage/v1/object/public/images/public/c12b5210-afc7-404a-9d66-a2245408456e/Portfolio.png', 'Tailwind,Docker,BasicAuth,Supabase,TypeScript,NextJS}', 'https://github.com/diderikk/Blog', 'https://diderikk.dev', null), 
('daa57ed2-5ee4-420b-9e09-6a0989488373', 'Stun server', 'StunServer is a STUN-server implemented to return your public IPv4-address when you send a STUN-request. It currently only accepts UDP-protocol messages, but TCP may be added later. StunServer parses and validates your request and returns an adequate response. Responses will either return your public IPv4-address or return a STUN error code.', 'https://yafxgqyvfrhuqxefmwvc.supabase.co/storage/v1/object/public/images/public/daa57ed2-5ee4-420b-9e09-6a0989488373/StunServer.png', 'WebRTC,UDP,STUN,C++', 'https://github.com/diderikk/StunServer', null, null);

