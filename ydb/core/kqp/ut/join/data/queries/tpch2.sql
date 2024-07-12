PRAGMA TablePathPrefix='/Root/test/ds';

select * from customer_address 
where ca_county in ('Fillmore County','McPherson County','Bonneville County','Boone County','Brown County') 
