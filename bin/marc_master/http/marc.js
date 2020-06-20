function show_table_rows(class_name)
{
	var rows = document.getElementsByTagName("tr");
	for(i=0; i<rows.length; i++)
	{
		if(rows[i].className == class_name)
			rows[i].style.display = "";
	}
}

function hide_table_rows(class_name)
{
	var rows = document.getElementsByTagName("tr");
	for(i=0; i<rows.length; i++)
	{
		if(rows[i].className == class_name)
			rows[i].style.display = "none";
	}		
}

function checkbox_onclick(cb_id, class_name)
{
	
	if(document.getElementById(cb_id).checked==true) //Òþ²Ø
	{
		hide_table_rows(class_name);
	}
	else //ÏÔÊ¾
	{
		show_table_rows(class_name);
	}
}
