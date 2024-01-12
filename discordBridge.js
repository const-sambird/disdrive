const express = require('express')
const app = express()

const { Client, Events, GatewayIntentBits } = require('discord.js')
const config = require('./config.json')
const discordClient = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.MessageContent, GatewayIntentBits.GuildMessageReactions, GatewayIntentBits.GuildMessages ]})

discordClient.once(Events.ClientReady, c => {
    console.log('logged in to discord');
})

app.get('/read', (req, res) => {
    discordClient.channels.fetch(config.data_channel_id).then(channel => {
        channel.messages.fetch({ limit: 100 }).then(messages => {
            console.log('read ok')
            messages.reverse()
            let ret = ''
            messages.forEach((message, id) => {
                ret += message.content
            })
            return res.send(ret)
        })
    })
})

app.get('/write', (req, res) => {
    console.log('block: ' + req.query.block)
    console.log('data:  ' + req.query.data)
    discordClient.channels.fetch(config.data_channel_id).then(channel => {
        channel.messages.fetch({ limit: 100 }).then(messages => {
            console.log('fetch ok')
            messages.reverse()
            messages.at(Number(req.query.block)).edit(req.query.data).then(m => {
                console.log('write ok')
                return res.sendStatus(200);
            })
        })
    })
})

discordClient.login(config.discord.token)
app.listen(3000)
