jQuery(document).ready(function(){
		var waypoint = jQuery('#jmm-counter-181').waypoint(function() {
			jQuery('#jmm-counter-181 .jmm-timer').countTo({
		formatter: function (value, options) {
			return value.toFixed(options.decimals).toString().split(".").join(",").replace(/(\d)(?=(\d{3})+(?!\d))/g, "$1,");
		}
	});
			this.destroy();
		}, {
			offset: 'bottom-in-view'
		});
	});

	</script>


<!-- META FOR IOS & HANDHELD -->
	<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no"/>
	<style type="text/stylesheet">
		@-webkit-viewport   { width: device-width; }
		@-moz-viewport      { width: device-width; }
		@-ms-viewport       { width: device-width; }
		@-o-viewport        { width: device-width; }
		@viewport           { width: device-width; }
	</style>
