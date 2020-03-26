({
	uloop: {
		defines: {
			eventQueueSize: "number",
			dataQueueSize: "number",
			listenerTimeLimit: "number",
			metadataNameSize: "number",
			metadataEnabled: "boolean",
			statisticsEnabled: "boolean",
			_strict: true
		},
		events: [{
			name: "string",
			"metadata?": "string",
			_strict: true
		}, "+"],
		listeners: [{
			function: "string",
			"metadata?": "string",
			events: ["string", "*"],
			_strict: true
		}, "+"],
		prefix: "string"
	}
})
