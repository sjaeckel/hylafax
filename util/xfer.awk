$3 == "SEND"	{ gsub("\"", "", $4);
		  sendpages[$4] += $6;
		  sendtime[$4] += $7;
		}
END		{ OFS="\t";
		  print "Pages", "Time (minutes)", "Account";
		  for (i in sendpages)
		      print sendpages[i], sendtime[i], i;
		}
